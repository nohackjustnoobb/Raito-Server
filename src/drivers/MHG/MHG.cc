#include "MHG.hpp"
#include "../../models/Manga.hpp"
#include "lz-string.hpp"
#include "utils.cc"

#include <cpr/cpr.h>
#include <ctime>
#include <fmt/format.h>
#include <locale>
#include <mutex>
#include <nlohmann/json.hpp>
#include <thread>

#define TIMEOUT_LIMIT 5000

#define PULL_DURATION 30

#define CHECK_TIMEOUT()                                                        \
  if (r.status_code == 0)                                                      \
    throw "Request timeout";

using namespace MHG_utils;

MHG::MHG() {
  id = "MHG";
  version = "0.1.0-beta.7";

  supportSuggestion = false;
  recommendedChunkSize = 5;
  timeout = 10;

  for (const auto &pair : categoryText)
    supportedCategories.push_back(pair.first);

  proxyHeaders = {{"referer", "https://tw.manhuagui.com"}};
}

vector<Manga *> MHG::getManga(vector<string> ids, bool showDetails,
                              string proxy) {
  vector<Manga *> result;

  bool isError = false;
  vector<thread> threads;
  mutex mutex;
  auto fetchId = [&](const string &id, vector<Manga *> &result) {
    try {
      cpr::Url url{baseUrl + "comic/" + id + "/"};
      cpr::Response r = proxy == "" ? cpr::Get(url, cpr::Timeout{TIMEOUT_LIMIT})
                                    : cpr::Get(url, cpr::Timeout{TIMEOUT_LIMIT},
                                               cpr::Proxies{{"https", proxy}});

      CHECK_TIMEOUT()

      Node *body = new Node(r.text);

      Manga *manga = extractDetails(body, id, showDetails);
      delete body;

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

  return result;
};

vector<string> MHG::getChapter(string id, string extraData, string proxy) {
  vector<string> result;

  cpr::Url url{baseUrl + "comic/" + extraData + "/" + id + ".html"};
  cpr::Response r = proxy == "" ? cpr::Get(url, cpr::Timeout{TIMEOUT_LIMIT})
                                : cpr::Get(url, cpr::Timeout{TIMEOUT_LIMIT},
                                           cpr::Proxies{{"https", proxy}});

  CHECK_TIMEOUT()

  re2::StringPiece input(r.text);
  string encoded, valuesString, len1String, len2String;

  if (RE2::PartialMatch(
          input,
          RE2(R"((?m)^.*\}\(\'(.*)\',(\d*),(\d*),\'([\w|\+|\/|=]*)\'.*$)"),
          &encoded, &len1String, &len2String, &valuesString)) {
    int len1, len2;
    len1 = stoi(len1String);
    len2 = stoi(len2String);

    valuesString = convert.to_bytes(
        lzstring::decompressFromBase64(convert.from_bytes(valuesString)));

    result = decodeChapters(encoded, len1, len2, valuesString);
  } else {
    throw "Can't parse the data";
  }

  return result;
};

vector<Manga *> MHG::getList(Category category, int page, Status status) {
  string url = baseUrl + "list/";
  string filter;

  if (category != All)
    filter += categoryText[category];

  if (status != Any) {
    if (category != All)
      filter += "_";
    filter += status == OnGoing ? "lianzai" : "wanjie";
  }

  if (!filter.empty())
    filter += "/";

  // add filter
  url += filter;
  // add page
  url += "index_p" + to_string(page) + ".html";

  cpr::Response r = cpr::Get(cpr::Url{url}, cpr::Timeout{TIMEOUT_LIMIT});

  CHECK_TIMEOUT()

  Node *body = new Node(r.text);

  vector<Manga *> result;
  vector<Node *> items = body->findAll("#contList > li");

  for (Node *node : items) {
    result.push_back(extractManga(node));
  }

  delete body;
  releaseMemory(items);

  return result;
};

vector<Manga *> MHG::search(string keyword, int page) {
  cpr::Response r = cpr::Get(
      cpr::Url{baseUrl + "s/" + keyword + "_p" + to_string(page) + ".html"},
      cpr::Timeout{TIMEOUT_LIMIT});

  CHECK_TIMEOUT()

  Node *body = new Node(r.text);

  vector<Manga *> result;
  vector<Node *> items = body->findAll("div.book-result > ul > li.cf");

  for (Node *node : items)
    result.push_back(extractManga(node));

  delete body;
  releaseMemory(items);

  return result;
};

vector<PreviewManga> MHG::getUpdates(string proxy) {
  cpr::Session session;
  session.SetUrl(cpr::Url{
      fmt::format("https://tw.manhuagui.com/update/d{}.html", PULL_DURATION)});
  session.SetTimeout(cpr::Timeout{TIMEOUT_LIMIT});

  if (proxy != "")
    session.SetProxies(cpr::Proxies{{"https", proxy}, {"http", proxy}});

  cpr::Response r = session.Get();

  CHECK_TIMEOUT()

  Node *body = new Node(r.text);

  vector<PreviewManga> result;
  vector<Node *> items =
      body->findAll("div.latest-cont > div.latest-list > ul > li");
  for (Node *node : items) {
    Node *details = node->find("a");
    string id = details->getAttribute("href");
    id = id = id.substr(7, id.length() - 8);

    Node *latestNode = details->find("span.tt");
    string latest = latestNode->text();
    RE2::GlobalReplace(&latest, RE2("更新至|共"), "");
    latest = strip(latest);

    delete latestNode;
    delete details;

    result.push_back({id, latest});
  }

  delete body;

  return result;
}

bool MHG::isLatestEqual(string value1, string value2) {
  // create a new copy
  string normalizedValue1 = string(value1);
  string normalizedValue2 = string(value2);

  // TODO some title like "19卷 報告" and "19卷" should be normalized also
  RE2::GlobalReplace(&normalizedValue1, RE2("第"), "");
  RE2::GlobalReplace(&normalizedValue2, RE2("第"), "");

  return ActiveDriver::isLatestEqual(normalizedValue1, normalizedValue2);
}

bool MHG::checkOnline() {
  return cpr::Get(cpr::Url{baseUrl}, cpr::Timeout{TIMEOUT_LIMIT}).status_code !=
         0;
};

Manga *MHG::extractDetails(Node *node, const string &id,
                           const bool showDetails) {
  Node *details = node->find("div.book-cover");

  Node *thumbnailNode = details->find("img");
  string thumbnail = "https:" + thumbnailNode->getAttribute("src");
  string title = thumbnailNode->getAttribute("alt");
  delete thumbnailNode;

  Node *isEndedNode = details->tryFind("span.finish");
  bool isEnded = isEndedNode != nullptr;
  delete isEndedNode;

  Node *latestNode = details->find("span.text");
  string latest = latestNode->text();
  RE2::GlobalReplace(&latest, RE2("更新至："), "");
  latest = strip(latest);
  delete latestNode;

  delete details;
  if (!showDetails) {
    return new Manga(this, id, title, thumbnail, latest, isEnded);
  }

  details = node->find("ul.detail-list > :nth-child(2)");

  vector<Node *> categoriesNode = details->findAll("span:nth-child(1) > a");
  vector<Category> categories;

  string temp;
  for (Node *node : categoriesNode) {
    temp = node->getAttribute("href");
    temp = temp.substr(6, temp.length() - 7);

    for (const auto &pair : categoryText) {
      if (pair.second == temp) {
        categories.push_back(pair.first);
      }
    }
  }

  releaseMemory(categoriesNode);

  vector<Node *> authorNode = details->findAll("span:nth-child(2) > a");
  vector<string> authors;

  for (Node *node : authorNode) {
    authors.push_back(node->text());
  }

  releaseMemory(authorNode);
  delete details;

  Node *descriptionNode = node->find("div.book-intro > #intro-all");
  string description = descriptionNode->text();

  delete descriptionNode;

  auto pushChapters = [](Node *node, vector<Chapter> &chapters) {
    vector<Node *> chaptersGroupNode = node->findAll("ul");
    vector<Node *> chaptersNode;

    vector<Node *> temp;
    for (int i = chaptersGroupNode.size() - 1; i >= 0; --i) {
      temp = chaptersGroupNode[i]->findAll("li > a");
      chaptersNode.insert(chaptersNode.end(), temp.begin(), temp.end());
    }

    releaseMemory(chaptersGroupNode);

    for (Node *chapter : chaptersNode) {
      string id = strip(chapter->getAttribute("href"));
      RE2::GlobalReplace(&id, RE2("\\.html|\\/comic\\/.*\\/"), "");

      chapters.push_back({strip(chapter->getAttribute("title")), id});
    }

    releaseMemory(chaptersNode);
  };

  vector<Chapter> serial;
  vector<Chapter> extra;

  vector<Node *> chaptersNode;
  Node *tryAdult = node->tryFind("#__VIEWSTATE");
  Node *hiddenElem;

  if (tryAdult == nullptr) {
    chaptersNode = node->findAll("div.chapter-list");
  } else {
    hiddenElem = new Node(convert.to_bytes(lzstring::decompressFromBase64(
        convert.from_bytes(tryAdult->getAttribute("value")))));
    chaptersNode = hiddenElem->findAll("div.chapter-list");
  }

  if (chaptersNode.size() >= 1)
    pushChapters(chaptersNode.at(0), serial);

  if (chaptersNode.size() >= 2)
    for (size_t i = 1; i < chaptersNode.size(); i++)
      pushChapters(chaptersNode.at(i), extra);

  releaseMemory(chaptersNode);
  if (tryAdult != nullptr) {
    delete hiddenElem;
    delete tryAdult;
  }

  return new DetailsManga(this, id, title, thumbnail, latest, authors, isEnded,
                          description, categories, {serial, extra, id});
}

Manga *MHG::extractManga(Node *node) {
  Node *details = node->find("a");
  string title = details->getAttribute("title");
  string id = details->getAttribute("href");
  id = id = id.substr(7, id.length() - 8);

  Node *thumbnailNode = details->find("img");
  string thumbnail = "https:" + thumbnailNode->getAttribute("src");
  delete thumbnailNode;

  Node *isEndedNode = details->tryFind("span.fd");
  bool isEnded = isEndedNode != nullptr;
  delete isEndedNode;

  Node *latestNode = details->find("span.tt");
  string latest = latestNode->text();
  RE2::GlobalReplace(&latest, RE2(R"(更新至|\[完\]|\[全\]|共)"), "");
  latest = strip(latest);
  delete latestNode;

  delete details;

  return new Manga(this, id, title, thumbnail, latest, isEnded);
}

vector<string> MHG::decodeChapters(const string &encoded, const int &len1,
                                   const int &len2,
                                   const string &valuesString) {
  string decoded = decompress(encoded, len1, len2, valuesString);

  RE2::GlobalReplace(&decoded, RE2(R"(SMH\.imgData\(|\)\.preInit\(\);)"), "");
  nlohmann::json data = nlohmann::json::parse(decoded);

  string baseUrl = "https://i.hamreus.com" + data["path"].get<string>();

  vector<string> result;
  for (const string &item : data["files"].get<vector<string>>())
    result.push_back(baseUrl + item);

  return result;
}
