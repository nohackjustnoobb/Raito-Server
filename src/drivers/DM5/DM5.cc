
#include "DM5.hpp"
#include "../../models/Manga.hpp"

#include "utils.cc"

#include <mutex>
#include <re2/re2.h>
#include <thread>

#include <iostream>

#define TIMEOUT_LIMIT 5000

#define CHECK_TIMEOUT()                                                        \
  if (r.status_code == 0)                                                      \
    throw "Request timeout";

using namespace DM5_utils;

DM5::DM5() {
  id = "DM5";
  version = "0.1.0-beta.1";

  supportSuggestion = true;
  recommendedChunkSize = 10;
  for (const auto &pair : categoryId) {
    supportedCategories.push_back(pair.first);
  }

  proxyHeaders = {{"referer", "http://www.dm5.com/dm5api/"}};
}

vector<Manga *> DM5::getManga(vector<string> ids, bool showDetails) {
  vector<Manga *> result;

  bool isError = false;
  vector<thread> threads;
  mutex mutex;
  auto fetchId = [&](const string &id, vector<Manga *> &result) {
    try {
      cpr::Response r =
          cpr::Get(cpr::Url{baseUrl + "manhua-" + id}, header,
                   cpr::Cookies{{"isAdult", "1"}}, cpr::Timeout{TIMEOUT_LIMIT});

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

vector<string> DM5::getChapter(string id, string extraData) {
  cpr::Response r = cpr::Get(cpr::Url{"https://www.manhuaren.com/m" + id},
                             header, cpr::Timeout{TIMEOUT_LIMIT});

  CHECK_TIMEOUT()

  vector<string> result;
  re2::StringPiece input(r.text);
  string encoded, valuesString, len1String, len2String;

  if (RE2::PartialMatch(
          input,
          RE2(R"((?m)^.*\}\(\'(.*)\',(\d*),(\d*),\'([\w|\+|\/|=]*)\'.*$)"),
          &encoded, &len1String, &len2String, &valuesString)) {
    int len1, len2;
    len1 = stoi(len1String);
    len2 = stoi(len2String);

    result = decodeChapters(encoded, len1, len2, valuesString);
  }

  return result;
};

vector<Manga *> DM5::getList(Category category, int page, Status status) {
  cpr::Response r = cpr::Get(
      cpr::Url{baseUrl + "manhua-list-tag" + to_string(categoryId[category]) +
               "-st" + to_string(status) + "-p" + to_string(page)},
      cpr::Timeout{TIMEOUT_LIMIT}, header);

  CHECK_TIMEOUT()

  vector<Manga *> result;

  Node *body = new Node(r.text);
  vector<Node *> items = body->findAll("ul.col7.mh-list > li > div.mh-item");
  for (Node *node : items)
    result.push_back(extractManga(node));

  releaseMemory(items);
  delete body;

  return result;
};

vector<string> DM5::getSuggestion(string keyword) {
  cpr::Response r = cpr::Get(
      cpr::Url{baseUrl + "search.ashx?t=" +
               DM5_utils::urlEncode(chineseConverter.toSimplified(keyword))},
      cpr::Timeout{TIMEOUT_LIMIT}, header);

  CHECK_TIMEOUT()

  vector<string> result;

  Node *body = new Node(r.text);
  vector<Node *> items = body->findAll("span.left");
  for (Node *node : items) {
    result.push_back(node->text());
  }
  releaseMemory(items);
  delete body;

  return result;
};

vector<Manga *> DM5::search(string keyword, int page) {
  cpr::Response r = cpr::Get(
      cpr::Url{baseUrl + "search?title=" +
               DM5_utils::urlEncode(chineseConverter.toSimplified(keyword)) +
               "&page=" + to_string(page)},
      header, cpr::Timeout{TIMEOUT_LIMIT});

  CHECK_TIMEOUT()

  vector<Manga *> result;
  Node *body = new Node(r.text);

  Node *huge = body->tryFind("div.banner_detail_form");
  if (huge != nullptr) {
    // extract thumbnail
    Node *thumbnailNode = huge->find("img");
    string thumbnail = thumbnailNode->getAttribute("src");
    delete thumbnailNode;

    // extract title & id
    Node *titleNode = huge->find("p.title > a");
    string title = titleNode->text();
    string id = titleNode->getAttribute("href");
    id = id = id.substr(8, id.length() - 9);
    delete titleNode;

    // extract isEnded
    Node *isEndedNode = huge->find("span.block > span");
    bool isEnded = isEndedNode->text() != "连载中";
    delete isEndedNode;

    // extract latest
    Node *latestNode = huge->find("div.bottom > a.btn-2");
    string latest = strip(latestNode->getAttribute("title"));
    size_t pos = latest.find(title + " ");
    if (pos != string::npos) {
      latest.replace(pos, (title + " ").length(), "");
    }
    delete latestNode;

    result.push_back(new Manga(this, id, title, thumbnail, latest, isEnded));
  }

  vector<Node *> items = body->findAll("ul.col7.mh-list > li > div.mh-item");
  for (Node *node : items) {
    result.push_back(extractManga(node));
  }
  releaseMemory(items);
  delete body;

  return result;
};

bool DM5::checkOnline() {
  return cpr::Get(cpr::Url{baseUrl}, header, cpr::Timeout{10000}).status_code !=
         0;
};

Manga *DM5::extractManga(Node *node) {
  // extract title & id
  Node *titleNode = node->find("h2.title > a");
  string title = titleNode->text();
  string id = titleNode->getAttribute("href");
  id = id.substr(8, id.length() - 9);

  // release memory allocated
  delete titleNode;

  // extract thumbnail
  Node *thumbnailNode = node->find("p.mh-cover");

  string thumbnailStyle = thumbnailNode->getAttribute("style");
  string thumbnail;

  re2::StringPiece url;
  if (RE2::PartialMatch(thumbnailStyle, RE2("url\\(([^)]+)\\)"), &url))
    thumbnail = string(url.data(), url.size());

  // release memory allocated
  delete thumbnailNode;

  // extract isEnded & latest
  Node *latestNode = node->find("p.chapter");
  Node *temp = latestNode->find("span");
  bool isEnded = temp->text() == "完结";

  // release memory allocated
  delete temp;

  temp = latestNode->find("a");
  string latest = strip(temp->text());

  // release memory allocated
  delete temp;

  return new Manga(this, id, title, thumbnail, latest, isEnded);
}

Manga *DM5::extractDetails(Node *node, const string &id,
                           const bool showDetails) {
  Node *infoNode = node->find("div.info");

  // extract title
  Node *titleNode = infoNode->find("p.title");
  string title = strip(titleNode->content());

  delete titleNode;

  // extract authors
  vector<Node *> authorNode = infoNode->findAll("p.subtitle > a");
  vector<string> authors;

  for (Node *node : authorNode)
    authors.push_back(node->text());

  // release memory allocated
  releaseMemory(authorNode);

  // extract thumbnail
  Node *thumbnailNode = node->find("div.cover > img");
  string thumbnail = thumbnailNode->getAttribute("src");

  delete thumbnailNode;

  // extract isEnded & latest
  Node *latestNode = node->find("div.detail-list-title > span.s > span > a");
  string latest = strip(latestNode->text());

  // release memory allocated
  delete latestNode;

  Node *tipNode = node->find("p.tip");
  Node *isEndedNode = tipNode->find("span.block > span");
  bool isEnded = isEndedNode->text() != "连载中";

  delete isEndedNode;

  if (!showDetails) {
    delete tipNode;
    delete infoNode;

    return new Manga(this, id, title, thumbnail, latest, isEnded);
  }

  Node *descriptionNode = infoNode->find("p.content");
  string description = descriptionNode->text();
  size_t pos = description.find("[+展开]");
  if (pos != string::npos) {
    description.replace(pos, 12, "");
  }
  pos = description.find("[-折叠]");
  if (pos != string::npos) {
    description.replace(pos, 9, "");
  }

  delete infoNode;

  vector<Node *> categoriesNode = tipNode->findAll("a");
  vector<Category> categories;

  string temp;
  for (Node *node : categoriesNode) {
    temp = node->getAttribute("href");
    temp = temp.substr(8, temp.length() - 9);

    if (categoryText.find(temp) != categoryText.end()) {
      categories.push_back(categoryText[temp]);
    }
  }

  releaseMemory(categoriesNode);
  delete tipNode;

  auto pushChapters = [](Node *node, vector<Chapter> &chapters) {
    vector<Node *> chaptersNode = node->findAll("a");
    for (Node *chapter : chaptersNode) {
      string id = strip(chapter->getAttribute("href"));
      id = id.substr(2, id.length() - 3);

      chapters.push_back({strip(chapter->content()), id});
    }

    releaseMemory(chaptersNode);
  };

  vector<Chapter> serial;
  vector<Chapter> extra;

  Node *serialNode = node->tryFind("#detail-list-select-1");
  if (serialNode != nullptr)
    pushChapters(serialNode, serial);
  delete serialNode;

  Node *extraNode = node->tryFind("#detail-list-select-2");
  if (extraNode != nullptr)
    pushChapters(extraNode, serial);
  delete extraNode;

  extraNode = node->tryFind("#detail-list-select-3");
  if (extraNode != nullptr)
    pushChapters(extraNode, serial);
  delete extraNode;

  return new DetailsManga(this, id, title, thumbnail, latest, authors, isEnded,
                          description, categories, {serial, extra, ""});
}

vector<string> DM5::decodeChapters(const string &encoded, const int &len1,
                                   const int &len2,
                                   const string &valuesString) {
  string decoded = decompress(encoded, len1, len2, valuesString);

  RE2::GlobalReplace(&decoded, RE2(R"((.+\[\\')|\];)"), "");

  vector<string> result = DM5_utils::split(decoded, RE2(R"(\\',\\'|\\')"));

  return result;
}