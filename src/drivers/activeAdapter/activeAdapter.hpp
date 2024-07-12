#pragma once

#include "../../models/activeDriver.hpp"
#include "../../models/localDriver.hpp"

// This is the adapter of the ActiveDriver. It will handles the updates and
// caching of the database.
class ActiveAdapter : public LocalDriver {
public:
  ActiveAdapter(ActiveDriver *driver);

  vector<string> getChapter(string id, string extraData) override;

  void applyConfig(json config) override;

  ~ActiveAdapter();

private:
  ActiveDriver *driver;
  int counter = 300;
  vector<string> waitingList;
  vector<string> proxies;
  int proxyCount = 0;
  std::mutex mutex;

  void mainLoop();
};