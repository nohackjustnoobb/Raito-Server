#include "ImagesManager.hpp"
#include "../utils/utils.hpp"

#include "crow.h"
#include <chrono>
#include <filesystem>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fstream>
#include <re2/re2.h>
#include <thread>

void ImagesManager::add(string id, cpr::Header headers) {
  settings[id] = headers;
}

void ImagesManager::setBaseUrl(string url) { this->url = url; }

void ImagesManager::setProxy(string proxy) { this->proxy = new string(proxy); }

string ImagesManager::getPath(string id, string genre, string dest) {
  filesystem::create_directories(fmt::format("../image/{}/{}", id, genre));

  string removeHost = dest;
  RE2::GlobalReplace(&removeHost, R"(https?:\/\/([^\/]+))", "");

  string hash = md5(removeHost);

  string path = fmt::format("../image/{}/{}/{}", id, genre, hash);
  if (!filesystem::exists(path)) {
    std::ofstream ofs(path);
    ofs << dest << endl;
    ofs.close();
  }

  return fmt::format("{}image/{}/{}/{}", url, id, genre, hash);
}

vector<string> ImagesManager::getImage(string id, string genre, string hash,
                                       bool useBase64) {
  string path = fmt::format("../image/{}/{}/{}", id, genre, hash);
  if (!filesystem::exists(path))
    throw "Image cannot be found";

  std::ifstream ifs(path);

  if (!ifs.good())
    throw "Image cannot be read";

  string url;
  getline(ifs, url);

  // get the format of the image
  string type;
  RE2::PartialMatch(url, R"(\.(jpg|jpeg|png|gif|bmp|tiff|webp))", &type);

  if (!crow::mime_types.contains(type))
    throw "Image format not supported";

  // check if cached
  string img;
  if (getline(ifs, img)) {
    ifs.close();

    if (useBase64)
      return {"txt", fmt::format("data:{};base64, {}",
                                 crow::mime_types.at(type), img)};

    return {type, crow::utility::base64decode(img, img.size())};
  }

  ifs.close();

  // if not cached, fetch the image
  cpr::Response r;
  if (this->proxy != nullptr)
    r = cpr::Get(cpr::Url(url), settings[id], cpr::Timeout{5000},
                 cpr::Proxies{{"https", *this->proxy}, {"http", *this->proxy}});
  else
    r = cpr::Get(cpr::Url(url), settings[id], cpr::Timeout{5000});

  if (r.status_code == 0)
    throw "Request timeout";

  string encoded = crow::utility::base64encode(r.text, r.text.size());

  // cache the image
  std::ofstream ofs(path, std::ios_base::app);
  ofs << encoded;
  ofs.close();

  if (useBase64)
    return {"txt", fmt::format("data:{};base64, {}", crow::mime_types.at(type),
                               encoded)};

  return {type, r.text};
}

void ImagesManager::setInterval(string interval) {
  if (this->interval != nullptr)
    return;

  // extract the value and unit
  int value;
  char unit;
  stringstream ss(interval);
  ss >> value;
  ss >> unit;
  if (ss.fail())
    return;

  // convert unit to minutes
  switch (unit) {
  case 'h':
    value *= 60;
    break;
  case 'd':
    value *= 1440;
    break;
  }

  this->interval = new int(value);
  thread(&ImagesManager::cleaner, this).detach();
}

void ImagesManager::cleaner() {
  string path = "../image";

  function<void(fs::path)> processFile = [](const fs::path &filePath) {
    std::ifstream inFile(filePath);
    if (inFile.is_open()) {
      std::string url;
      std::getline(inFile, url);
      inFile.close();

      std::ofstream outFile(filePath, std::ios::trunc);
      if (outFile.is_open()) {
        outFile << url << endl;
        outFile.close();
      }
    }
  };

  function<void(fs::path)> processDirectory =
      [&processDirectory, &processFile](const fs::path &dirPath) {
        for (const auto &entry : fs::directory_iterator(dirPath)) {
          if (entry.is_directory())
            processDirectory(entry.path());
          else if (entry.is_regular_file())
            processFile(entry.path());
        }
      };

  while (true) {
    CROW_LOG_INFO << "Clearing Image Cache";
    if (fs::exists(path) && fs::is_directory(path))
      processDirectory(path);

    this_thread::sleep_for(chrono::minutes(*this->interval));
  }
}

ImagesManager imagesManager;