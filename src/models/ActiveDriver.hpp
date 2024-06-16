#pragma once

#include "baseDriver.hpp"

using namespace std;

// This the base class for active drivers.
// All active drivers should extend this class and be adapted by active
// adapters.
// Only the pure virtual methods in this file are required to be overridden.
// The methods with the same name as BaseDriver should feature the same except
// they should be able to use the given proxy to fetch the manga.
class ActiveDriver : public BaseDriver {
public:
  // The timeout of each fetch request.
  int timeout;

  // Get the updated manga
  virtual vector<PreviewManga> getUpdates(string proxy = "") = 0;

  virtual vector<Manga *> getManga(vector<string> ids, bool showDetails,
                                   string proxy) = 0;

  virtual vector<string> getChapter(string id, string extraData,
                                    string proxy) = 0;

  // This function is used to specify the appropriate way to compare the latest
  // title.
  virtual bool isLatestEqual(string value1, string value2) {
    return value1.find(value2) != string::npos ||
           value2.find(value1) != string::npos;
  }

  vector<Manga *> getManga(vector<string> ids, bool showDetails) override {
    return this->getManga(ids, showDetails, "");
  };

  vector<string> getChapter(string id, string extraData) override {
    return this->getChapter(id, extraData, "");
  };
};