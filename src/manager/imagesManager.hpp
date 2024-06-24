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

// This class is used to manage the images
class ImagesManager {
public:
  // This should not be called directly.
  void add(string id, cpr::Header headers);

  // This is a setter for the base URL, which will be used to construct the
  // proxy URL. If not specified, it will used the detected base URL.
  void setBaseUrl(string url);

  // This is a setter for the proxy, which will the proxy for fetching the
  // images.
  void setProxy(string proxy);

  // This is a setter for the interval between clearing the cache.
  void setInterval(string interval);

  // This should not be called directly.
  void cleaner();

  // Get the full path to the image.
  string getPath(const string &id, const string &genre, const string &dest,
                 const string &baseUrl);

  // Get the full path to a local image.
  string getLocalPath(const string &id, const string &genre, const string &dest,
                      const string &baseUrl);

  // Get the image data.
  vector<string> getImage(const string &id, const string &genre,
                          const string &hash, bool asBase64);

  // Save a image to the local storage
  string saveImage(const string &id, const string &genre, const string &image);

  // Remove the image from the local storage
  void deleteImage(const string &id, const string &genre, const string &hash);

private:
  map<string, cpr::Header, CaseInsensitiveCompare> settings;
  string *proxy;
  string url;
  int *interval;
};

extern ImagesManager imagesManager;