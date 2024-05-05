#pragma once

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;
using namespace std;

struct CaseInsensitiveCompare {
  bool operator()(const string &str1, const string &str2) const {
    string str1_lower, str2_lower;
    str1_lower.resize(str1.size());
    str2_lower.resize(str2.size());

    transform(str1.begin(), str1.end(), str1_lower.begin(), ::tolower);
    transform(str2.begin(), str2.end(), str2_lower.begin(), ::tolower);

    return str1_lower < str2_lower;
  }
};

class ImagesManager {
public:
  void add(string id, cpr::Header headers);

  void setBaseUrl(string url);

  void setProxy(string proxy);

  void setInterval(string interval);

  void cleaner();

  string getPath(const string &id, const string &genre, const string &dest,
                 const string &baseUrl);

  vector<string> getImage(const string &id, const string &genre,
                          const string &hash, bool asBase64);

private:
  map<string, cpr::Header, CaseInsensitiveCompare> settings;
  string *proxy;
  string url;
  int *interval;
};

extern ImagesManager imagesManager;