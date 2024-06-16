#pragma once

#include "../manager/imagesManager.hpp"
#include "common.hpp"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;
using namespace std;

// This is the base class for drivers.
// All passive drivers should extend this class and override every pure virtual
// methods.
class BaseDriver {
public:
  // The id of the driver.
  string id;
  // The version of the driver.
  string version;
  // The recommended chunk size for fetching the manga information.
  int recommendedChunkSize = 0;
  // Determines whether the driver supports suggestions.
  bool supportSuggestion;
  // The supported categories for the driver.
  vector<Category> supportedCategories;
  // The proxy headers that the image manager will use to fetch images.
  cpr::Header proxyHeaders;

  // Return the mangas for the given ids.
  // Return DetailManga if the showDetails is true.
  virtual vector<Manga *> getManga(vector<string> ids, bool showDetails) = 0;

  // Return a list of urls for the given chapter id and extra data.
  virtual vector<string> getChapter(string id, string extraData) = 0;

  // Return a list of manga for the given information.
  // The size of the returned list should be limited to 50.
  virtual vector<Manga *> getList(Category category, int page,
                                  Status status) = 0;

  // Return a list of suggestions for the given keyword.
  // The size of the returned list should be limited to 5.
  virtual vector<string> getSuggestion(string keyword) {
    return vector<string>();
  };

  // Return a list of result for the given keyword.
  // The size of the returned list should be limited to 50.
  virtual vector<Manga *> search(string keyword, int page) = 0;

  // The function should return true only if the source is working.
  virtual bool checkOnline() = 0;

  // This function is used to control how the driver handles the proxy images.
  // It should not be overridden unless you know what you are doing.
  virtual string useProxy(const string &dest, const string &genre,
                          const string &baseUrl) {
    return imagesManager.getPath(id, genre, dest, baseUrl);
  }

  // The corresponding config will be passed to this function.
  virtual void applyConfig(json config) {}
};
