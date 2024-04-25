#include "crow.hpp"

#include "../manager/DriversManager.hpp"
#include "../manager/ImagesManager.hpp"
#include "../models/Manga.hpp"
#include "../utils/converter.hpp"
#include "../utils/utils.hpp"

#include "crow.h"
#include "crow/middlewares/cors.h"
#include <fmt/format.h>
#include <nlohmann/json.hpp>

#define GET_DRIVER()                                                           \
  char *driverId = req.url_params.get("driver");                               \
  if (driverId == nullptr)                                                     \
    return crow::response(400, "json",                                         \
                          R"({"error":"\"driver\" is missing."})");            \
                                                                               \
  BaseDriver *driver = driversManager.get(driverId);                           \
  if (driver == nullptr)                                                       \
    return crow::response(404, "json", R"({"error":"Driver is not found."})");

#define GET_PROXY()                                                            \
  bool proxy = false;                                                          \
  char *tryProxy = req.url_params.get("proxy");                                \
  if (tryProxy != nullptr) {                                                   \
    try {                                                                      \
      proxy = std::atoi(tryProxy) == 1;                                        \
    } catch (...) {                                                            \
    }                                                                          \
  }

#define GET_PAGE()                                                             \
  int page = 1;                                                                \
  char *tryPage = req.url_params.get("page");                                  \
  if (tryPage != nullptr) {                                                    \
    try {                                                                      \
      int parsedPage = std::atoi(tryPage);                                     \
      if (parsedPage > 0)                                                      \
        page = parsedPage;                                                     \
    } catch (...) {                                                            \
    }                                                                          \
  }

#define GET_KEYWORD()                                                          \
  char *keyword = req.url_params.get("keyword");                               \
  if (keyword == nullptr)                                                      \
    return crow::response(400, "json",                                         \
                          R"({"error":"\"keyword\" is missing."})");

namespace crowServer {
string *webpageUrl;
string *accessKey;
string serverVersion;
Converter converter;
}; // namespace crowServer

using namespace crowServer;
using json = nlohmann::json;

crow::response getServerInfo() {
  vector<string> drivers;
  for (const auto &driver : driversManager.getAll())
    drivers.push_back(driver->id);

  json info;

  info["version"] = serverVersion;
  info["availableDrivers"] = drivers;

  return crow::response("json", info.dump());
}

crow::response getDriverInfo(const crow::request &req) {
  GET_DRIVER()

  vector<string> categories;
  for (Category category : driver->supportedCategories)
    categories.push_back(categoryToString(category));

  json info;
  info["supportedCategories"] = categories;
  info["recommendedChunkSize"] = driver->recommendedChunkSize;
  info["supportSuggestion"] = driver->supportSuggestion;
  info["version"] = driver->version;

  return crow::response("json", info.dump());
}

crow::response getDriverOnline(const crow::request &req) {
  vector<BaseDriver *> drivers;

  char *driverIds = req.url_params.get("drivers");
  if (driverIds != nullptr) {
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

  return crow::response("json", result.dump());
}

crow::response getList(const crow::request &req) {
  GET_DRIVER()

  GET_PAGE()

  GET_PROXY()

  Category category = All;
  char *tryCategory = req.url_params.get("category");
  if (tryCategory != nullptr)
    category = stringToCategory(tryCategory);

  Status status = Any;
  char *tryStatus = req.url_params.get("status");
  if (tryStatus != nullptr) {
    try {
      int parsedStatus = std::atoi(tryStatus);
      if (parsedStatus >= 0 && parsedStatus <= 2)
        status = (Status)parsedStatus;
    } catch (...) {
    }
  }

  try {
    vector<Manga *> mangas = driver->getList(category, page, status);
    json result = json::array();
    for (Manga *manga : mangas) {
      if (proxy)
        manga->useProxy();

      result.push_back(manga->toJson());
    }

    releaseMemory(mangas);

    return crow::response("json", result.dump());
  } catch (...) {
    return crow::response(
        400, "json",
        R"({"error": "An unexpected error occurred when trying to get list."})");
  }
}

crow::response getManga(const crow::request &req) {
  GET_DRIVER()

  vector<string> ids;
  if (req.method == crow::HTTPMethod::GET) {
    char *tryIds = req.url_params.get("ids");
    if (tryIds != nullptr) {
      try {
        ids = split(tryIds, ",");
      } catch (...) {
      }
    }
  } else {
    try {
      json body = json::parse(req.body);
      ids = body["ids"].get<vector<string>>();
    } catch (...) {
    }
  }

  if (ids.empty())
    return crow::response(
        400, "json", R"({"error":"\"ids\" is missing or cannot be parsed."})");

  GET_PROXY()

  bool showAll = false;
  char *tryShowAll = req.url_params.get("show-all");
  if (tryShowAll != nullptr) {
    try {
      showAll = std::atoi(tryShowAll) == 1;
    } catch (...) {
    }
  }

  try {
    vector<Manga *> mangas = driver->getManga(ids, showAll);
    json result = json::array();
    for (Manga *manga : mangas) {
      if (proxy)
        manga->useProxy();

      result.push_back(manga->toJson());
    }

    releaseMemory(mangas);

    return crow::response(200, "json", result.dump());
  } catch (...) {
    return crow::response(
        400, "json",
        R"({"error": "An unexpected error occurred when trying to get manga."})");
  }
}

crow::response getChapter(const crow::request &req) {
  GET_DRIVER()

  char *id = req.url_params.get("id");
  if (id == nullptr)
    return crow::response(400, "json", R"({"error":"\"id\" is missing."})");

  GET_PROXY()

  string extraData;
  char *tryExtraData = req.url_params.get("extra-data");
  if (tryExtraData != nullptr)
    extraData = tryExtraData;

  try {
    vector<string> urls = driver->getChapter(id, extraData);
    json result = json::array();
    if (proxy)
      for (const string &url : urls)
        result.push_back(driver->useProxy(url, "manga"));
    else
      result = urls;

    return crow::response("json", result.dump());
  } catch (...) {
    return crow::response(
        400, "json",
        R"({"error": "An unexpected error occurred when trying to get chapters."})");
  }
}

crow::response getSuggestion(const crow::request &req) {
  GET_DRIVER()

  GET_KEYWORD()

  try {
    json result = driver->getSuggestion(keyword);

    return crow::response("json", result.dump());
  } catch (...) {
    return crow::response(
        400, "json",
        R"({"error": "An unexpected error occurred when trying to get suggestions."})");
  }
}

crow::response getSearch(const crow::request &req) {
  GET_DRIVER()

  GET_KEYWORD()

  GET_PAGE()

  GET_PROXY()

  try {
    vector<Manga *> mangas = driver->search(keyword, page);
    json result = json::array();
    for (Manga *manga : mangas) {
      if (proxy)
        manga->useProxy();

      result.push_back(manga->toJson());
    }

    releaseMemory(mangas);

    return crow::response("json", result.dump());
  } catch (...) {
    return crow::response(
        400, "json",
        R"({"error": "An unexpected error occurred when trying to search manga."})");
  }
}

crow::response getShare(const crow::request &req) {
  if (webpageUrl == nullptr)
    return crow::response(404);

  crow::response resp = crow::response();

  try {
    // get driver & id
    char *driverId = req.url_params.get("driver");
    char *id = req.url_params.get("id");
    if (id == nullptr || driverId == nullptr)
      throw R"("driver" or "id" not found)";

    BaseDriver *driver = driversManager.get(driverId);
    if (driver == nullptr)
      throw "Driver not found";

    // get proxy
    bool proxy = false;
    char *tryProxy = req.url_params.get("proxy");
    if (tryProxy != nullptr)
      try {
        proxy = std::atoi(tryProxy) == 1;
      } catch (...) {
      }

    // get the manga
    DetailsManga *manga = (DetailsManga *)(driver->getManga({id}, true).at(0));
    if (proxy)
      manga->useProxy();

    // generate the webpage
    resp.set_header("Content-Type", crow::mime_types.at("html"));
    resp.body = fmt::format(
        R"(<!doctype html><html lang=en><meta content="0;url={}share?driver={}&id={}"http-equiv=refresh><meta content="{}" property=og:title><meta content="{}" property=og:image><meta content="{}" property=og:description><meta content=website property=og:type>)",
        *webpageUrl, driverId, id, converter.toTraditional(manga->title),
        manga->thumbnail, converter.toTraditional(manga->description));

    return resp;
  } catch (...) {
  }

  // if any unexpected error is encountered
  resp.redirect(*webpageUrl);
  return resp;
}

crow::response getImage(const crow::request &req, string id, string genre,
                        string hash) {
  try {
    bool useBase64 = false;
    char *tryBase64 = req.url_params.get("base64");
    if (tryBase64 != nullptr)
      useBase64 = std::atoi(tryBase64) == 1;

    vector<string> result = imagesManager.getImage(id, genre, hash, useBase64);

    crow::response resp = crow::response(result.at(0), result.at(1));
    string cacheControl = req.get_header_value("Cache-Control");

    resp.add_header("Cache-Control",
                    cacheControl != "" ? cacheControl : "max-age=43200");

    return resp;
  } catch (...) {
    return crow::response(404);
  }
}

struct AccessGuard {
  struct context {};

  void before_handle(crow::request &req, crow::response &res,
                     context & /*ctx*/) {

    stringstream ss;
    ss << &req;

    string ip = req.get_header_value("X-Real-IP");
    if (ip == "")
      ip = req.remote_ip_address;

    log("Crow",
        {{"request", ss.str()},
         {"client", ip},
         {"method", crow::method_strings[(int)req.method]},
         {"path", req.url}},
        fmt::color::light_golden_rod_yellow);

    if (accessKey != nullptr &&
        !(req.method == crow::HTTPMethod::OPTIONS ||
          req.get_header_value("Access-Key") == *accessKey ||
          (RE2::FullMatch(req.raw_url, R"(^\/(image|share).*$)")))) {
      res.write(R"({"error":"\"Access-Key\" is not found or matched."})");
      res.add_header("Content-Type", "application/json");
      res.code = 403;
      res.end();
    }
  }

  void after_handle(crow::request &req, crow::response &res,
                    context & /*ctx*/) {

    stringstream ss;
    ss << &req;

    string ip = req.get_header_value("X-Real-IP");
    if (ip == "")
      ip = req.remote_ip_address;

    log("Crow",
        {{"response", ss.str()},
         {"client", ip},
         {"status", to_string(res.code)},
         {"path", req.url}},
        isSuccess(res.code) ? fmt::color::light_green : fmt::color::red);
  }
};

void startCrowServer(string *_webpageUrl, string *_accessKey) {
  webpageUrl = _webpageUrl;
  accessKey = _accessKey;
  serverVersion = string(getenv("RAITO_SERVER_VERSION"));
  serverVersion += " (Crow)";

  crow::App<AccessGuard, crow::CORSHandler> app;
  app.loglevel(crow::LogLevel::Critical);

  // CORS
  auto &cors = app.get_middleware<crow::CORSHandler>();
  cors.global().methods(crow::HTTPMethod::GET, crow::HTTPMethod::POST,
                        crow::HTTPMethod::OPTIONS);

  CROW_ROUTE(app, "/")(getServerInfo);
  CROW_ROUTE(app, "/driver")(getDriverInfo);
  CROW_ROUTE(app, "/driver/online")(getDriverOnline);

  CROW_ROUTE(app, "/list")(getList);
  CROW_ROUTE(app, "/manga")
      .methods(crow::HTTPMethod::GET, crow::HTTPMethod::POST)(getManga);
  CROW_ROUTE(app, "/chapter")(getChapter);
  CROW_ROUTE(app, "/suggestion")(getSuggestion);
  CROW_ROUTE(app, "/search")(getSearch);

  CROW_ROUTE(app, "/share")(getShare);
  CROW_ROUTE(app, "/image/<string>/<string>/<string>")(getImage);

  int port = atoi(getenv("RAITO_SERVER_PORT"));
  log("Crow", fmt::format("Listening on Port {}", port));
  app.port(port).multithreaded().run();
}