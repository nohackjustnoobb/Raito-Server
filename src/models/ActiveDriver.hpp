#pragma once

#include "BaseDriver.hpp"

using namespace std;

class ActiveDriver : public BaseDriver {
public:
  int timeout;

  virtual vector<PreviewManga> getUpdates(string proxy = "") = 0;

  virtual vector<Manga *> getManga(vector<string> ids, bool showDetails,
                                   string proxy) = 0;

  virtual vector<string> getChapter(string id, string extraData,
                                    string proxy) = 0;

  vector<Manga *> getManga(vector<string> ids, bool showDetails) override {
    return this->getManga(ids, showDetails, "");
  };

  vector<string> getChapter(string id, string extraData) override {
    return this->getChapter(id, extraData, "");
  };
};