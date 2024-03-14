#pragma once

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;
using namespace std;

class ImagesManager {
public:
  void add(string id, cpr::Header headers);

  void setBaseUrl(string url);

  void setProxy(string proxy);

  void setInterval(string interval);

  void cleaner();

  string getPath(string id, string genre, string dest);

  vector<string> getImage(string id, string genre, string hash, bool asBase64);

private:
  map<string, cpr::Header> settings;
  string *proxy;
  string url;
  int *interval;
};

extern ImagesManager imagesManager;