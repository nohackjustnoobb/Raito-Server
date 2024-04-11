#include "ImagesManager.hpp"
#include "../utils/utils.hpp"

#include "crow.h"
#include "md5.h"
#include <FreeImage.h>
#include <chrono>
#include <filesystem>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fstream>
#include <re2/re2.h>
#include <thread>

#define SAVE_FORMAT "webp"
#define FIF_FORMAT FIF_WEBP

void ImagesManager::add(string id, cpr::Header headers) {
  settings[id] = headers;
}

void ImagesManager::setBaseUrl(string url) { this->url = url; }

void ImagesManager::setProxy(string proxy) { this->proxy = new string(proxy); }

string ImagesManager::getPath(string id, string genre, string dest) {
  filesystem::create_directories(fmt::format("../image/{}/{}", id, genre));

  string removeHost = dest;
  RE2::GlobalReplace(&removeHost, R"(https?:\/\/([^\/]+))", "");

  string hash = MD5()(removeHost);

  string path = fmt::format("../image/{}/{}/{}.src", id, genre, hash);
  if (!filesystem::exists(path)) {
    std::ofstream ofs(path);
    ofs << dest;
    ofs.close();
  }

  return fmt::format("{}image/{}/{}/{}", url, id, genre, hash);
}

vector<string> ImagesManager::getImage(string id, string genre, string hash,
                                       bool useBase64) {
  string path = fmt::format("../image/{}/{}/{}.src", id, genre, hash);
  if (!filesystem::exists(path))
    throw "Image cannot be found";

  // check the cache
  string imagePath =
      fmt::format("../image/{}/{}/{}.{}", id, genre, hash, SAVE_FORMAT);
  if (filesystem::exists(imagePath)) {
    std::ifstream imageFile(imagePath, std::ios::binary);

    std::ostringstream oss;
    oss << imageFile.rdbuf();
    imageFile.close();
    string imageData = oss.str();

    if (useBase64)
      return {"txt",
              fmt::format(
                  "data:{};base64, {}", crow::mime_types.at(SAVE_FORMAT),
                  crow::utility::base64encode(imageData, imageData.size()))};

    return {SAVE_FORMAT, imageData};
  }

  std::ifstream ifs(path);
  string url;
  getline(ifs, url);

  // fetch the image
  cpr::Response r;
  if (this->proxy != nullptr)
    r = cpr::Get(cpr::Url(url), settings[id], cpr::Timeout{5000},
                 cpr::Proxies{{"https", *this->proxy}, {"http", *this->proxy}});
  else
    r = cpr::Get(cpr::Url(url), settings[id], cpr::Timeout{5000});

  if (r.status_code >= 300 || r.status_code < 200)
    throw "Error fetching image";

  if (r.status_code == 0)
    throw "Request timeout";

  // load the image
  FIMEMORY *hmem = FreeImage_OpenMemory((BYTE *)&r.text[0], r.text.length());
  FREE_IMAGE_FORMAT fif = FreeImage_GetFileTypeFromMemory(hmem, 0);
  FIBITMAP *dib = FreeImage_LoadFromMemory(fif, hmem);

  // check if the image was loaded successfully
  if (dib == nullptr)
    throw "Failed to load image";

  // cache the image
  FIBITMAP *converted_dib = FreeImage_ConvertTo24Bits(dib);
  if (!FreeImage_Save(FIF_FORMAT, converted_dib, imagePath.c_str()))
    throw "Failed to save image";

  FreeImage_Unload(converted_dib);
  FreeImage_Unload(dib);
  FreeImage_CloseMemory(hmem);

  return this->getImage(id, genre, hash, useBase64);
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

  function<void(filesystem::path)> processFile =
      [](const filesystem::path &filePath) {
        if (filePath.extension() != ".src")
          filesystem::remove(filePath);
      };

  function<void(filesystem::path)> processDirectory =
      [&processDirectory, &processFile](const filesystem::path &dirPath) {
        for (const auto &entry : filesystem::directory_iterator(dirPath)) {
          if (entry.is_directory())
            processDirectory(entry.path());
          else if (entry.is_regular_file())
            processFile(entry.path());
        }
      };

  while (true) {
    log("ImagesManager", "Clearing Image Cache");
    if (filesystem::exists(path) && filesystem::is_directory(path))
      processDirectory(path);

    this_thread::sleep_for(chrono::minutes(*this->interval));
  }
}

ImagesManager imagesManager;