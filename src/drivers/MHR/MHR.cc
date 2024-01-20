
#include "MHR.hpp"
#include "../../models/Manga.hpp"
#include "utils.cc"

#include <algorithm>
#include <codecvt>

#include <mutex>
#include <nlohmann/json.hpp>
#include <thread>

using namespace MHR_utils;

MHR::MHR() {
  id = "MHR";
  version = "0.1.0-beta.0";

  supportSuggestion = true;
  for (const auto &pair : categoryId) {
    supportedCategories.push_back(pair.first);
  }
  proxyHeaders = {{"referer", "http://www.dm5.com/dm5api/"}};
}

vector<Manga *> MHR::getManga(vector<string> ids, bool showDetails) {
  vector<Manga *> result;

  if (showDetails) {
    bool isError = false;
    vector<thread> threads;
    mutex mutex;
    auto fetchId = [&](const string &id, vector<Manga *> &result) {
      try {
        map<string, string> query = {
            {"mangaId", id},
            {"mangaDetailVersion", ""},
        };

        query.insert(baseQuery.begin(), baseQuery.end());
        query["gsn"] = hash(query);

        cpr::Response r =
            cpr::Get(cpr::Url{baseUrl + "v1/manga/getDetail"},
                     mapToParameters(query), header, cpr::Timeout{5000});
        json data = json::parse(r.text);

        Manga *manga = convertDetails(data["response"]);
        lock_guard<std::mutex> guard(mutex);
        result.push_back(manga);
      } catch (...) {
        isError = true;
      }
    };

    // create threads
    for (const string &id : ids) {
      threads.push_back(thread(fetchId, ref(id), ref(result)));
    }

    // wait for threads to finish
    for (auto &th : threads) {
      th.join();
    }

    if (isError)
      throw "Failed to fetch manga";

  } else {

    map<string, string> query;
    query.insert(baseQuery.begin(), baseQuery.end());

    // construct body
    json body;
    body["mangaCoverimageType"] = 1;
    body["bookIds"] = json::array();
    body["somanIds"] = json::array();
    body["mangaIds"] = ids;
    string dumpBody = body.dump();

    query["gsn"] = hash(query, dumpBody);

    // add content-type to headers temporarily
    header["Content-Type"] = "application/json";
    cpr::Response r =
        cpr::Post(cpr::Url{baseUrl + "v2/manga/getBatchDetail"},
                  mapToParameters(query), header, cpr::Body{dumpBody});

    // remove content-type from headers
    header.erase("Content-Type");

    // parse the data
    json data = json::parse(r.text);

    for (const json &manga : data["response"]["mangas"]) {
      result.push_back(convert(manga));
    }
  }

  return result;
};

vector<string> MHR::getChapter(string id, string extraData) {
  map<string, string> query = {
      {"mangaSectionId", id}, {"mangaId", extraData}, {"netType", "1"},
      {"loadreal", "1"},      {"imageQuality", "2"},
  };

  query.insert(baseQuery.begin(), baseQuery.end());
  query["gsn"] = hash(query);

  cpr::Response r =
      cpr::Get(cpr::Url{baseUrl + "v1/manga/getRead"}, mapToParameters(query),
               header, cpr::Timeout{5000});

  json data = json::parse(r.text)["response"];

  string baseUrl = data["hostList"].get<vector<string>>().at(0);
  vector<string> result;
  string urlQuery = data["query"].get<string>();

  for (const json &path : data["mangaSectionImages"]) {
    string url = baseUrl + path.get<string>() + urlQuery;

    result.push_back(url);
  }

  return result;
};

vector<Manga *> MHR::getList(Category category, int page, Status status) {

  map<string, string> query = {
      {"subCategoryType", "0"},
      {"subCategoryId", to_string(categoryId[category])},
      {"start", to_string((page - 1) * 50)},
      {"status", to_string(status)},
      {"limit", "50"},
      {"sort", "0"},
  };

  query.insert(baseQuery.begin(), baseQuery.end());
  query["gsn"] = hash(query);

  cpr::Response r =
      cpr::Get(cpr::Url{baseUrl + "v2/manga/getCategoryMangas"},
               mapToParameters(query), header, cpr::Timeout{5000});
  json data = json::parse(r.text);

  vector<Manga *> result;
  for (const json &manga : data["response"]["mangas"]) {
    result.push_back(convert(manga));
  }

  return result;
};

vector<string> MHR::getSuggestion(string keyword) {
  keyword.erase(remove(keyword.begin(), keyword.end(), '/'), keyword.end());

  map<string, string> query = {
      {"keywords", chineseConverter.toSimplified(keyword)},
      {"mh_is_anonymous", "0"},
  };

  query.insert(baseQuery.begin(), baseQuery.end());
  query["gsn"] = hash(query);

  cpr::Response r =
      cpr::Get(cpr::Url{baseUrl + "v1/search/getKeywordMatch"},
               mapToParameters(query), header, cpr::Timeout{5000});

  // parse & extract the data
  json data = json::parse(r.text);
  vector<string> result;

  for (const json &item : data["response"]["items"]) {
    result.push_back(item["mangaName"].get<string>());
  }

  return result;
};

vector<Manga *> MHR::search(string keyword, int page) {
  keyword.erase(remove(keyword.begin(), keyword.end(), '/'), keyword.end());

  map<string, string> query = {
      {"keywords", chineseConverter.toSimplified(keyword)},
      {"start", to_string((page - 1) * 50)},
      {"limit", "50"},
  };

  query.insert(baseQuery.begin(), baseQuery.end());
  query["gsn"] = hash(query);

  cpr::Response r =
      cpr::Get(cpr::Url{baseUrl + "v1/search/getSearchManga"},
               mapToParameters(query), header, cpr::Timeout{5000});
  json data = json::parse(r.text);

  vector<string> result;
  for (const json &manga : data["response"]["result"]) {
    result.push_back(to_string(manga["mangaId"].get<int>()));
  }

  return getManga(result, false);
};

bool MHR::checkOnline() {
  return cpr::Get(cpr::Url{baseUrl}, cpr::Timeout{5000}).status_code != 0;
};

// create a MD5 hash from a list of strings
string MHR::hash(vector<string> &list) {
  string result;

  for (const string &str : list)
    result += str;

  result = MHR_utils::urlEncode(result);

  return md5(result);
}

string MHR::hash(map<string, string> query) {
  vector<string> flatQuery;

  flatQuery.push_back(hashKey);
  flatQuery.push_back("GET");

  vector<string> keys;
  for (const auto &pair : query)
    keys.push_back(pair.first);
  sort(keys.begin(), keys.end());

  for (const auto &key : keys) {
    flatQuery.push_back(key);
    flatQuery.push_back(query[key]);
  }

  flatQuery.push_back(hashKey);

  return hash(flatQuery);
}

string MHR::hash(map<string, string> query, string body) {
  vector<string> flatQuery;

  flatQuery.push_back(hashKey);
  flatQuery.push_back("POST");

  flatQuery.push_back("body");
  flatQuery.push_back(body);

  vector<string> keys;
  for (const auto &pair : query)
    keys.push_back(pair.first);
  sort(keys.begin(), keys.end());

  for (const auto &key : keys) {
    flatQuery.push_back(key);
    flatQuery.push_back(query[key]);
  }

  flatQuery.push_back(hashKey);

  return hash(flatQuery);
}

Manga *MHR::convert(const json &data) {
  return new Manga(this, to_string(data["mangaId"].get<int>()),
                   data["mangaName"].get<string>(),
                   extractThumbnail(data["mangaCoverimageUrl"].get<string>()),
                   data.contains("mangaNewestContent")
                       ? data["mangaNewestContent"].get<string>()
                       : data["mangaNewsectionName"].get<string>(),
                   data["mangaIsOver"].get<int>() == 1);
}

Manga *MHR::convertDetails(const json &data) {
  vector<Chapter> serial;
  vector<Chapter> extra;
  string id = to_string(data["mangaId"].get<int>());

  auto pushChapters = [](const json &chaptersData, vector<Chapter> &chapters) {
    for (const json &chapter : chaptersData) {
      chapters.push_back({chapter["sectionName"].get<string>(),
                          to_string(chapter["sectionId"].get<int>())});
    }
  };

  pushChapters(data["mangaWords"], serial);
  pushChapters(data["mangaEpisode"], extra);
  pushChapters(data["mangaRolls"], extra);

  vector<Category> categories;
  string rawCategoriesText = data["mangaTheme"].get<string>();

  for (const auto &pair : categoryText) {
    if (rawCategoriesText.find(pair.first) != string::npos) {
      categories.push_back(pair.second);
    }
  }

  return new DetailsManga(
      this, id, data["mangaName"].get<string>(),
      extractThumbnail(data["mangaPicimageUrl"].get<string>()),
      data["mangaNewsectionName"].get<string>(),
      data["mangaAuthors"].get<vector<string>>(),
      data["mangaIsOver"].get<int>() == 1, data["mangaIntro"].get<string>(),
      categories, {serial, extra, id});
}

string MHR::extractThumbnail(const string &url) {
  // make a copy
  string thumbnail = url;
  // swap the source to a faster alternative
  RE2::GlobalReplace(&thumbnail, RE2("cdndm5.com"), "cdnmanhua.net");

  return thumbnail;
}

vector<string> MHR::extractAuthors(const string authorsString) {
  vector<string> authors = MHR_utils::split(authorsString, RE2("，|,|、| |/"));

  authors.erase(remove_if(authors.begin(), authors.end(),
                          [](string str) {
                            return str.find_first_not_of(" \t\n\r") ==
                                   string::npos;
                          }),
                authors.end());

  return authors;
}