#include "ImagesManager.hpp"
#include "../utils.hpp"

#include "crow.h"
#include <filesystem>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fstream>
#include <re2/re2.h>

void ImagesManager::add(string id, cpr::Header headers) {
  settings[id] = headers;
}

void ImagesManager::setBaseUrl(string url) { this->url = url; }

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
  cpr::Response r = cpr::Get(cpr::Url(url), settings[id]);
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

ImagesManager imagesManager;