#include "drogon.hpp"

#include "../drivers/selfContained/selfContained.hpp"
#include "../manager/accessGuard.hpp"
#include "../manager/driversManager.hpp"
#include "../models/manga.hpp"
#include "../utils/base64.hpp"
#include "../utils/converter.hpp"
#include "../utils/log.hpp"
#include "../utils/mimeTypes.h"
#include "../utils/utils.hpp"

#include <drogon/drogon.h>

#define JSON_RESPONSE_WITH_CODE(str, code)                                     \
  HttpResponsePtr resp = HttpResponse::newHttpResponse();                      \
  resp->setContentTypeCode(CT_APPLICATION_JSON);                               \
  resp->setStatusCode(code);                                                   \
  resp->setBody(str);                                                          \
  return callback(resp);

#define JSON_RESPONSE(str) JSON_RESPONSE_WITH_CODE(str, k200OK)

#define JSON_404_RESPONSE(str) JSON_RESPONSE_WITH_CODE(str, k404NotFound)

#define JSON_400_RESPONSE(str) JSON_RESPONSE_WITH_CODE(str, k400BadRequest)

#define GET_DRIVER()                                                           \
  string driverId = req->getParameter("driver");                               \
  bool isAdmin = RE2::FullMatch(req->path(), R"(^\/admin.*$)");                \
  if (driverId == "" && !isAdmin) {                                            \
    JSON_404_RESPONSE(R"({"error":"\"driver\" is missing."})")                 \
  }                                                                            \
  if (driversManager.cmsId != nullptr && isAdmin)                              \
    driverId = *driversManager.cmsId;                                          \
  BaseDriver *driver = driversManager.get(driverId);                           \
  if (driver == nullptr) {                                                     \
    JSON_404_RESPONSE(R"({"error":"Driver is not found."})")                   \
  }

#define GET_PROXY()                                                            \
  bool proxy = false;                                                          \
  string baseUrl;                                                              \
  string host = req->getHeader("host");                                        \
  if (!host.empty())                                                           \
    baseUrl =                                                                  \
        fmt::format("{}://{}/", isLocalIp(host) ? "http" : "https", host);     \
  string tryProxy = req->getParameter("proxy");                                \
  if (tryProxy != "") {                                                        \
    try {                                                                      \
      proxy = std::stoi(tryProxy) == 1;                                        \
    } catch (...) {                                                            \
    }                                                                          \
  }                                                                            \
  if (driversManager.cmsId != nullptr && driver->id == *driversManager.cmsId)  \
    proxy = true;

#define GET_PAGE()                                                             \
  int page = 1;                                                                \
  string tryPage = req->getParameter("page");                                  \
  if (tryPage != "") {                                                         \
    try {                                                                      \
      int parsedPage = std::stoi(tryPage);                                     \
      if (parsedPage > 0)                                                      \
        page = parsedPage;                                                     \
    } catch (...) {                                                            \
    }                                                                          \
  }

#define GET_KEYWORD()                                                          \
  string keyword = req->getParameter("keyword");                               \
  if (keyword == "") {                                                         \
    JSON_400_RESPONSE(R"({"error":"\"keyword\" is missing."})")                \
  }

#define CHECK_MODE()                                                           \
  if (accessGuard.mode != "token") {                                           \
    JSON_400_RESPONSE(                                                         \
        R"({"error":"AccessGuard is not set to "token" mode."})")              \
  }

namespace drogonServer {
string *webpageUrl;
string serverVersion;
Converter converter;
} // namespace drogonServer

using namespace drogonServer;
using namespace drogon;
using json = nlohmann::json;

auto getServerInfo = [](const HttpRequestPtr &req,
                        function<void(const HttpResponsePtr &)> &&callback) {
  vector<string> drivers;
  for (const auto &driver : driversManager.getAll())
    drivers.push_back(driver->id);

  json info;

  info["version"] = serverVersion;
  info["availableDrivers"] = drivers;

  JSON_RESPONSE(info.dump())

  callback(resp);
};

auto getDriverInfo = [](const HttpRequestPtr &req,
                        function<void(const HttpResponsePtr &)> &&callback) {
  GET_DRIVER()

  vector<string> genres;
  for (Genre genre : driver->supportedGenres)
    genres.push_back(genreToString(genre));

  json info;
  info["supportedGenres"] = genres;
  info["recommendedChunkSize"] = driver->recommendedChunkSize;
  info["supportSuggestion"] = driver->supportSuggestion;
  info["version"] = driver->version;

  JSON_RESPONSE(info.dump())
};

auto getDriverOnline = [](const HttpRequestPtr &req,
                          function<void(const HttpResponsePtr &)> &&callback) {
  vector<BaseDriver *> drivers;

  string driverIds = req->getParameter("drivers");
  if (driverIds != "") {
    BaseDriver *temp;
    for (const auto &id : split(driverIds, ",")) {
      temp = driversManager.get(id);
      if (temp != nullptr)
        drivers.push_back(temp);
    }
  } else {
    drivers = driversManager.getAll();
  }

  json result = json::object();
  vector<std::thread> threads;
  std::mutex mutex;

  auto testOnline = [&mutex, &result](BaseDriver *driver) {
    auto start = std::chrono::high_resolution_clock::now();
    bool online = driver->checkOnline();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::lock_guard<std::mutex> guard(mutex);
    result[driver->id]["online"] = online;
    result[driver->id]["latency"] = online ? (double)duration.count() : 0;
  };

  for (BaseDriver *driver : drivers)
    threads.push_back(std::thread(testOnline, driver));

  // wait for threads to finish
  for (auto &th : threads)
    th.join();

  JSON_RESPONSE(result.dump())
};

auto getList = [](const HttpRequestPtr &req,
                  function<void(const HttpResponsePtr &)> &&callback) {
  GET_DRIVER()

  GET_PAGE()

  GET_PROXY()

  Genre genre = All;
  string tryGenre = req->getParameter("genre");
  if (tryGenre != "")
    genre = stringToGenre(tryGenre);

  Status status = Any;
  string tryStatus = req->getParameter("status");
  if (tryStatus != "") {
    try {
      int parsedStatus = std::stoi(tryStatus);
      if (parsedStatus >= 0 && parsedStatus <= 2)
        status = (Status)parsedStatus;
    } catch (...) {
    }
  }

  try {
    vector<Manga *> mangas = driver->getList(genre, page, status);
    json result = json::array();
    for (Manga *manga : mangas) {
      if (proxy)
        manga->useProxy(baseUrl);

      result.push_back(manga->toJson());
    }

    releaseMemory(mangas);

    JSON_RESPONSE(result.dump())
  } catch (...) {
    JSON_400_RESPONSE(
        R"({"error": "An unexpected error occurred when trying to get list."})")
  }
};

auto getManga = [](const HttpRequestPtr &req,
                   function<void(const HttpResponsePtr &)> &&callback) {
  GET_DRIVER()

  vector<string> ids;
  if (req->getMethod() == Get) {
    string tryIds = req->getParameter("ids");
    if (tryIds != "") {
      try {
        ids = split(tryIds, ",");
      } catch (...) {
      }
    }
  } else {
    try {
      json body = json::parse(req->getBody());
      ids = body["ids"].get<vector<string>>();
    } catch (...) {
    }
  }

  if (ids.empty()) {
    JSON_400_RESPONSE(R"({"error":"\"ids\" is missing or cannot be parsed."})")
  }

  GET_PROXY()

  bool showAll = false;
  string tryShowAll = req->getParameter("show-all");
  if (tryShowAll != "") {
    try {
      showAll = std::stoi(tryShowAll) == 1;
    } catch (...) {
    }
  }

  try {
    vector<Manga *> mangas = driver->getManga(ids, showAll);

    json result = json::array();
    for (Manga *manga : mangas) {
      if (proxy)
        manga->useProxy(baseUrl);

      result.push_back(manga->toJson());
    }

    releaseMemory(mangas);

    JSON_RESPONSE(result.dump());
  } catch (...) {
    JSON_400_RESPONSE(
        R"({"error": "An unexpected error occurred when trying to get manga."})")
  }
};

auto getChapter = [](const HttpRequestPtr &req,
                     function<void(const HttpResponsePtr &)> &&callback) {
  GET_DRIVER()

  string id = req->getParameter("id");
  if (id == "") {
    JSON_400_RESPONSE(R"({"error":"\"id\" is missing."})")
  }

  GET_PROXY()

  string extraData = req->getParameter("extra-data");

  try {
    vector<string> urls = driver->getChapter(id, extraData);
    json result = json::array();
    if (proxy)
      for (const string &url : urls)
        result.push_back(driver->useProxy(url, "manga", baseUrl));
    else
      result = urls;

    JSON_RESPONSE(result.dump())
  } catch (...) {
    JSON_400_RESPONSE(
        R"({"error": "An unexpected error occurred when trying to get chapters."})")
  }
};

auto getSuggestion = [](const HttpRequestPtr &req,
                        function<void(const HttpResponsePtr &)> &&callback) {
  GET_DRIVER()

  GET_KEYWORD()

  try {
    json result = driver->getSuggestion(keyword);

    JSON_RESPONSE(result.dump())
  } catch (...) {
    JSON_400_RESPONSE(
        R"({"error": "An unexpected error occurred when trying to get suggestions."})")
  }
};

auto getSearch = [](const HttpRequestPtr &req,
                    function<void(const HttpResponsePtr &)> &&callback) {
  GET_DRIVER()

  GET_KEYWORD()

  GET_PAGE()

  GET_PROXY()

  try {
    vector<Manga *> mangas = driver->search(keyword, page);
    json result = json::array();
    for (Manga *manga : mangas) {
      if (proxy)
        manga->useProxy(baseUrl);

      result.push_back(manga->toJson());
    }

    releaseMemory(mangas);

    JSON_RESPONSE(result.dump())
  } catch (...) {
    JSON_400_RESPONSE(
        R"({"error": "An unexpected error occurred when trying to search manga."})")
  }
};

auto getShare = [](const HttpRequestPtr &req,
                   function<void(const HttpResponsePtr &)> &&callback) {
  if (webpageUrl == nullptr)
    return callback(HttpResponse::newNotFoundResponse());

  try {
    HttpResponsePtr resp = HttpResponse::newHttpResponse();

    // get driver & id
    string driverId = req->getParameter("driver");
    string id = req->getParameter("id");
    if (id == "" || driverId == "")
      throw R"("driver" or "id" not found)";

    BaseDriver *driver = driversManager.get(driverId);
    if (driver == nullptr)
      throw "Driver not found";

    // get proxy
    GET_PROXY()

    // get the manga
    DetailsManga *manga = (DetailsManga *)(driver->getManga({id}, true).at(0));
    if (proxy)
      manga->useProxy(baseUrl);

    // generate the webpage
    resp->setContentTypeCode(CT_TEXT_HTML);
    resp->setBody(fmt::format(
        R"(<!doctype html><html lang=en><meta content="0;url={}share?driver={}&id={}"http-equiv=refresh><meta content="{}" property=og:title><meta content="{}" property=og:image><meta content="{}" property=og:description><meta content=website property=og:type>)",
        *webpageUrl, driverId, id, converter.toTraditional(manga->title),
        manga->thumbnail, converter.toTraditional(manga->description)));

    delete manga;

    return callback(resp);
  } catch (...) {
    // if any unexpected error is encountered
    return callback(HttpResponse::newRedirectionResponse(*webpageUrl));
  }
};

auto getImage = [](const HttpRequestPtr &req,
                   function<void(const HttpResponsePtr &)> &&callback,
                   string id, string genre, string hash) {
  try {
    bool useBase64 = false;
    string tryBase64 = req->getParameter("base64");
    if (tryBase64 != "")
      useBase64 = std::stoi(tryBase64) == 1;

    vector<string> result = imagesManager.getImage(id, genre, hash, useBase64);

    HttpResponsePtr resp = HttpResponse::newHttpResponse();
    resp->setContentTypeString(mime_types.at(result[0]));
    resp->setBody(result[1]);

    string cacheControl = req->getHeader("Cache-Control");

    resp->addHeader("Cache-Control",
                    cacheControl.empty() ? "max-age=43200" : cacheControl);

    return callback(resp);
  } catch (...) {
    return callback(HttpResponse::newNotFoundResponse());
  }
};

auto createOrEditManga = [](const HttpRequestPtr &req,
                            function<void(const HttpResponsePtr &)>
                                &&callback) {
  GET_DRIVER()

  try {
    // Parse the body
    json body = json::parse(req->getBody());
    DetailsManga *manga = DetailsManga::fromJson(body);

    SelfContained *castedDriver = (SelfContained *)driver;

    Manga *result;
    if (req->getMethod() == Post)
      result = castedDriver->createManga(manga);
    else
      result = castedDriver->editManga(manga);

    GET_PROXY()
    result->useProxy(baseUrl);

    JSON_RESPONSE(result->toJson().dump());

    delete result;
  } catch (...) {
    JSON_400_RESPONSE(
        R"({"error": "An unexpected error occurred when trying to create/edit manga."})")
  }
};

auto deleteManga = [](const HttpRequestPtr &req,
                      function<void(const HttpResponsePtr &)> &&callback) {
  GET_DRIVER()

  try {
    string id = req->getParameter("id");
    if (id == "") {
      JSON_400_RESPONSE(R"({"error":"\"id\" is missing."})")
    }

    SelfContained *castedDriver = (SelfContained *)driver;

    castedDriver->deleteManga(id);

    HttpResponsePtr resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k204NoContent);

    return callback(resp);
  } catch (...) {
    JSON_400_RESPONSE(
        R"({"error": "An unexpected error occurred when trying to delete manga."})")
  }
};

auto createChapter = [](const HttpRequestPtr &req,
                        function<void(const HttpResponsePtr &)> &&callback) {
  GET_DRIVER()

  try {
    // Parse the body
    json body = json::parse(req->getBody());

    string keys[] = {"extraData", "title", "isExtra"};
    for (const auto &key : keys) {
      if (!body.contains(key)) {
        JSON_400_RESPONSE(R"({"error":"\")" + key + R"(\" is missing."})");
      }
    }

    SelfContained *castedDriver = (SelfContained *)driver;

    Chapters result = castedDriver->createChapter(
        body["extraData"].get<string>(), body["title"].get<string>(),
        body["isExtra"].get<bool>());

    JSON_RESPONSE(result.toJson().dump());
  } catch (...) {
    JSON_400_RESPONSE(
        R"({"error": "An unexpected error occurred when trying to create chapter."})")
  }
};

auto editChapters = [](const HttpRequestPtr &req,
                       function<void(const HttpResponsePtr &)> &&callback) {
  GET_DRIVER()

  try {
    // Parse the body
    json body = json::parse(req->getBody());
    Chapters chapters = Chapters::fromJson(body);

    SelfContained *castedDriver = (SelfContained *)driver;

    Chapters result = castedDriver->editChapters(chapters);

    JSON_RESPONSE(result.toJson().dump());
  } catch (...) {
    JSON_400_RESPONSE(
        R"({"error": "An unexpected error occurred when trying to edit chapters."})")
  }
};

auto deleteChapter = [](const HttpRequestPtr &req,
                        function<void(const HttpResponsePtr &)> &&callback) {
  GET_DRIVER()

  try {
    string id = req->getParameter("id");
    if (id == "") {
      JSON_400_RESPONSE(R"({"error":"\"id\" is missing."})")
    }

    string extraData = req->getParameter("extra-data");
    if (extraData == "") {
      JSON_400_RESPONSE(R"({"error":"\"extra-data\" is missing."})")
    }

    SelfContained *castedDriver = (SelfContained *)driver;

    castedDriver->deleteChapter(id, extraData);

    HttpResponsePtr resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k204NoContent);

    return callback(resp);
  } catch (...) {
    JSON_400_RESPONSE(
        R"({"error": "An unexpected error occurred when trying to delete chapter."})")
  }
};

auto uploadImage = [](const HttpRequestPtr &req,
                      function<void(const HttpResponsePtr &)> &&callback) {
  GET_DRIVER()

  try {
    string id = req->getParameter("id");
    if (id == "") {
      JSON_400_RESPONSE(R"({"error":"\"id\" is missing."})")
    }
    string extraData = req->getParameter("extra-data");

    string contentType = req->getHeader("content-type");

    SelfContained *castedDriver = (SelfContained *)driver;

    if (contentType.find("application/json") != string::npos &&
        extraData != "") {

      // get baseUrl
      GET_PROXY()

      json body = json::parse(req->getBody());
      json decoded = json::array();
      for (const auto &encoded : body)
        decoded.push_back(base64::from_base64(encoded.get<string>()));

      json result = json::array();
      vector<string> urls =
          castedDriver->uploadMangaImages(id, extraData, decoded);
      for (const string &url : urls)
        result.push_back(castedDriver->useProxy(url, "manga", baseUrl));

      JSON_RESPONSE(result.dump());
    } else {
      string image = string(req->bodyData(), req->bodyLength());
      if (contentType.find("text/plain") != string::npos)
        image = base64::from_base64(image);

      vector<string> result =
          extraData == ""
              ? castedDriver->uploadThumbnail(id, image)
              : castedDriver->uploadMangaImage(id, extraData, image);

      HttpResponsePtr resp = HttpResponse::newHttpResponse();
      resp->setContentTypeString(mime_types.at(result[0]));
      resp->setBody(result[1]);

      return callback(resp);
    }

  } catch (...) {
    JSON_400_RESPONSE(
        R"({"error": "An unexpected error occurred when trying to upload image."})")
  }
};

auto arrangeMangaImage = [](const HttpRequestPtr &req,
                            function<void(const HttpResponsePtr &)>
                                &&callback) {
  GET_DRIVER()

  try {
    string id = req->getParameter("id");
    if (id == "") {
      JSON_400_RESPONSE(R"({"error":"\"id\" is missing."})")
    }

    string extraData = req->getParameter("extra-data");
    if (extraData == "") {
      JSON_400_RESPONSE(R"({"error":"\"extra-data\" is missing."})")
    }

    // get baseUrl
    GET_PROXY()

    vector<string> urls = json::parse(req->getBody());

    SelfContained *castedDriver = (SelfContained *)driver;

    urls = castedDriver->arrangeMangaImage(id, extraData, urls);

    json result = json::array();
    for (const string &url : urls)
      result.push_back(castedDriver->useProxy(url, "manga", baseUrl));

    JSON_RESPONSE(result.dump())
  } catch (...) {
    JSON_400_RESPONSE(
        R"({"error": "An unexpected error occurred when trying to arrange image."})")
  }
};

auto deleteMangaImage = [](const HttpRequestPtr &req,
                           function<void(const HttpResponsePtr &)> &&callback) {
  GET_DRIVER()

  try {
    string id = req->getParameter("id");
    if (id == "") {
      JSON_400_RESPONSE(R"({"error":"\"id\" is missing."})")
    }

    string extraData = req->getParameter("extra-data");
    if (extraData == "") {
      JSON_400_RESPONSE(R"({"error":"\"extra-data\" is missing."})")
    }

    string url = req->getParameter("url");
    if (url == "") {
      JSON_400_RESPONSE(R"({"error":"\"url\" is missing."})")
    }

    SelfContained *castedDriver = (SelfContained *)driver;

    castedDriver->deleteMangaImage(id, extraData, url);

    HttpResponsePtr resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k204NoContent);

    return callback(resp);
  } catch (...) {
    JSON_400_RESPONSE(
        R"({"error": "An unexpected error occurred when trying to delete image."})")
  }
};

auto getToken = [](const HttpRequestPtr &req,
                   function<void(const HttpResponsePtr &)> &&callback) {
  CHECK_MODE()

  GET_PAGE()

  try {
    string id = req->getParameter("id");
    json result = accessGuard.getToken(id, page);

    JSON_RESPONSE(result.dump());
  } catch (...) {
    JSON_400_RESPONSE(
        R"({"error": "An unexpected error occurred when trying to get token."})")
  }
};

auto createToken = [](const HttpRequestPtr &req,
                      function<void(const HttpResponsePtr &)> &&callback) {
  CHECK_MODE()

  try {
    // Parse the body
    json body = json::parse(req->getBody());

    if (!body.contains("id"))
      body["id"] = "";

    string token = accessGuard.createToken(body["id"].get<string>());

    json result;
    result["id"] = body["id"].get<string>();
    result["token"] = token;

    JSON_RESPONSE(result.dump());
  } catch (...) {
    JSON_400_RESPONSE(
        R"({"error": "An unexpected error occurred when trying to create token."})")
  }
};

auto removeToken = [](const HttpRequestPtr &req,
                      function<void(const HttpResponsePtr &)> &&callback) {
  CHECK_MODE()

  try {
    string id = req->getParameter("id");
    if (id == "") {
      JSON_400_RESPONSE(R"({"error":"\"id\" is missing."})")
    }

    accessGuard.removeToken(id);

    HttpResponsePtr resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k204NoContent);

    return callback(resp);

  } catch (...) {
    JSON_400_RESPONSE(
        R"({"error": "An unexpected error occurred when trying to remove token."})")
  }
};

auto refreshToken = [](const HttpRequestPtr &req,
                       function<void(const HttpResponsePtr &)> &&callback) {
  CHECK_MODE()

  try {
    // Parse the body
    json body = json::parse(req->getBody());

    if (!body.contains("id"))
      body["id"] = "";

    string token = accessGuard.refreshToken(body["id"].get<string>());

    json result;
    result["id"] = body["id"].get<string>();
    result["token"] = token;

    JSON_RESPONSE(result.dump());
  } catch (...) {
    JSON_400_RESPONSE(
        R"({"error": "An unexpected error occurred when trying to refresh token."})")
  }
};

auto downloadManga = [](const HttpRequestPtr &req,
                        function<void(const HttpResponsePtr &)> &&callback) {
  GET_DRIVER()

  try {
    string id = req->getParameter("id");
    if (id == "") {
      JSON_400_RESPONSE(R"({"error":"\"id\" is missing."})")
    }

    bool asCBZ = req->getParameter("cbz") == "1";

    SelfContained *castedDriver = (SelfContained *)driver;

    vector<string> file = castedDriver->downloadManga(id, asCBZ);

    HttpResponsePtr resp =
        HttpResponse::newHttpResponse(k200OK, CT_APPLICATION_OCTET_STREAM);
    resp->setBody(file[1]);
    resp->addHeader("Content-Disposition",
                    fmt::format(R"(attachment; filename="{}")", file[0]));

    return callback(resp);
  } catch (...) {
    JSON_400_RESPONSE(
        R"({"error": "An unexpected error occurred when trying to download manga."})")
  }
};

auto uploadManga = [](const HttpRequestPtr &req,
                      function<void(const HttpResponsePtr &)> &&callback) {
  GET_DRIVER()

  try {
    SelfContained *castedDriver = (SelfContained *)driver;

    DetailsManga *result =
        castedDriver->uploadManga(string(req->bodyData(), req->bodyLength()));

    // get baseUrl
    GET_PROXY()
    result->useProxy(baseUrl);

    string json = result->toJson().dump();
    delete result;

    JSON_RESPONSE(json)
  } catch (...) {
    JSON_400_RESPONSE(
        R"({"error": "An unexpected error occurred when trying to upload manga."})")
  }
};

auto uploadChapter = [](const HttpRequestPtr &req,
                        function<void(const HttpResponsePtr &)> &&callback) {
  GET_DRIVER()

  try {
    string keys[] = {"extra-data", "title", "is-extra"};
    for (const auto &key : keys) {
      if (req->getParameter(key) == "") {
        JSON_400_RESPONSE(R"({"error":"\")" + key + R"(\" is missing."})");
      }
    }

    SelfContained *castedDriver = (SelfContained *)driver;

    Chapters result = castedDriver->uploadChapter(
        req->getParameter("extra-data"), req->getParameter("title"),
        req->getParameter("is-extra") == "1",
        string(req->bodyData(), req->bodyLength()));

    JSON_RESPONSE(result.toJson().dump());
  } catch (...) {
    JSON_400_RESPONSE(
        R"({"error": "An unexpected error occurred when trying to upload chapter."})")
  }
};

void startDrogonServer(int port, string *_webpageUrl) {
  webpageUrl = _webpageUrl;
  serverVersion = string(getenv("RAITO_SERVER_VERSION"));
  serverVersion += " (Drogon)";

  // setup logger
  app().registerPreRoutingAdvice([](const HttpRequestPtr &req) {
    stringstream ss;
    ss << req;

    string ip = req->getHeader("X-Real-IP");
    if (ip == "")
      ip = req->getPeerAddr().toIp();

    log("Drogon",
        {{"request", ss.str()},
         {"client", ip},
         {"method", req->methodString()},
         {"path", req->getPath()}},
        fmt::color::light_golden_rod_yellow);
  });

  app().registerPreSendingAdvice(
      [](const HttpRequestPtr &req, const HttpResponsePtr &resp) {
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Headers", "*");

        stringstream ss;
        ss << req;

        string ip = req->getHeader("X-Real-IP");
        if (ip == "")
          ip = req->getPeerAddr().toIp();

        log("Drogon",
            {{"response", ss.str()},
             {"client", ip},
             {"status", to_string(resp->statusCode())},
             {"path", req->getPath()}},
            isSuccess(resp->statusCode()) ? fmt::color::light_green
                                          : fmt::color::red);
      });

  // Access guard
  app().registerPreHandlingAdvice([](const HttpRequestPtr &req,
                                     AdviceCallback &&callback,
                                     AdviceChainCallback &&chainCallback) {
    string ip = req->getHeader("X-Real-IP");
    if (ip == "")
      ip = req->getPeerAddr().toIp();

    // Check if it is admin panel
    bool isAdminPanel = RE2::FullMatch(req->path(), R"(^\/admin.*$)");

    if (isAdminPanel &&
        accessGuard.verifyAdminKey(req->getHeader("Access-Key"), ip))
      return chainCallback();

    // Other requests
    if (!isAdminPanel && (RE2::FullMatch(req->path(), R"(^\/image.*$)") ||
                          accessGuard.verifyKey(req->getHeader("Access-Key"))))
      return chainCallback();

    // If not matching any of the above conditions
    HttpResponsePtr resp = HttpResponse::newHttpResponse();
    resp->setBody(R"({"error": "No Permission."})");
    resp->setContentTypeCode(CT_APPLICATION_JSON);
    resp->setStatusCode(k403Forbidden);
    callback(resp);
  });

  // register handlers
  app().registerHandler("/", getServerInfo, {Get, Options});
  app().registerHandler("/driver", getDriverInfo, {Get, Options});
  app().registerHandler("/driver/online", getDriverOnline, {Get, Options});

  app().registerHandler("/list", getList, {Get, Options});
  app().registerHandler("/manga", getManga, {Get, Post, Options});
  app().registerHandler("/chapter", getChapter, {Get, Options});
  app().registerHandler("/suggestion", getSuggestion, {Get, Options});
  app().registerHandler("/search", getSearch, {Get, Options});

  app().registerHandler("/share", getShare, {Get, Options});
  app().registerHandler("/image/{1}/{2}/{3}", getImage, {Get, Options});

  // Admin panel
  app().registerHandler("/admin", getDriverInfo, {Get, Options});
  app().registerHandler("/admin/list", getList, {Get, Options});
  app().registerHandler("/admin/manga", getManga, {Get, Post, Options});
  app().registerHandler("/admin/chapter", getChapter, {Get, Options});
  app().registerHandler("/admin/suggestion", getSuggestion, {Get, Options});
  app().registerHandler("/admin/search", getSearch, {Get, Options});
  // Editor
  app().registerHandler("/admin/manga/edit", createOrEditManga,
                        {Post, Put, Options});
  app().registerHandler("/admin/manga/edit", deleteManga, {Delete, Options});

  app().registerHandler("/admin/chapter/edit", createChapter, {Post, Options});
  app().registerHandler("/admin/chapter/edit", editChapters, {Put, Options});
  app().registerHandler("/admin/chapter/edit", deleteChapter,
                        {Delete, Options});

  app().registerHandler("/admin/image/edit", uploadImage, {Post, Options});
  app().registerHandler("/admin/image/edit", arrangeMangaImage, {Put, Options});
  app().registerHandler("/admin/image/edit", deleteMangaImage,
                        {Delete, Options});

  // Access Guard
  app().registerHandler("/admin/token", getToken, {Get, Options});
  app().registerHandler("/admin/token", createToken, {Post, Options});
  app().registerHandler("/admin/token", removeToken, {Delete, Options});
  app().registerHandler("/admin/token", refreshToken, {Put, Options});

  // Download and Upload Manga from Zip
  app().registerHandler("/admin/manga/download", downloadManga, {Get, Options});
  app().registerHandler("/admin/manga/upload", uploadManga, {Post, Options});
  app().registerHandler("/admin/chapter/upload", uploadChapter,
                        {Post, Options});

  log("Drogon", fmt::format("Listening on Port {}", port));
  app()
      .setClientMaxBodySize(1024 * 1024 * 1024)
      .setThreadNum(0)
      .addListener("0.0.0.0", port)
      .run();
}