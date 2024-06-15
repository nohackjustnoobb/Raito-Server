#pragma once

#include "../manager/ImagesManager.hpp"
#include "Common.hpp"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;
using namespace std;

class BaseDriver {
public:
  string id; // Should only contain numbers and alphabetic characters
  string version;
  int recommendedChunkSize = 0;
  bool supportSuggestion;
  vector<Category> supportedCategories;
  cpr::Header proxyHeaders;

  virtual vector<Manga *> getManga(vector<string> ids, bool showDetails) = 0;

  virtual vector<string> getChapter(string id, string extraData) = 0;

  virtual vector<Manga *> getList(Category category, int page,
                                  Status status) = 0;

  virtual vector<string> getSuggestion(string keyword) {
    return vector<string>();
  };

  virtual vector<Manga *> search(string keyword, int page) = 0;

  virtual bool checkOnline() = 0;

  virtual string useProxy(const string &dest, const string &genre,
                          const string &baseUrl) {
    return imagesManager.getPath(id, genre, dest, baseUrl);
  }

  virtual void applyConfig(json config) {}
};
