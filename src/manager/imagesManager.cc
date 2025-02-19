#include "imagesManager.hpp"
#include "../utils/base64.hpp"
#include "../utils/log.hpp"
#include "../utils/mimeTypes.h"
#include "../utils/utils.hpp"
#include "driversManager.hpp"

#include "md5.h"
#include <FreeImage.h>

#define SAVE_FORMAT "webp"
#define FIF_FORMAT FIF_WEBP

void ImagesManager::add(string id, cpr::Header headers) {
  settings[id] = headers;
}

void ImagesManager::setBaseUrl(string url) { this->url = url; }

void ImagesManager::setProxy(string proxy) { this->proxy = new string(proxy); }

string ImagesManager::getPath(const string &id, const string &genre,
                              const string &dest, const string &baseUrl) {
  filesystem::create_directories(fmt::format("../image/{}/{}", id, genre));

  string removeHost = dest;
  RE2::GlobalReplace(&removeHost, R"(https?:\/\/([^\/]+))", "");

  string hash = MD5()(removeHost);

  string path = fmt::format("../image/{}/{}/{}.src", id, genre, hash);
  string cachePath =
      fmt::format("../image/{}/{}/{}.{}", id, genre, hash, SAVE_FORMAT);
  if (!filesystem::exists(cachePath)) {
    std::ofstream ofs(path, ios::trunc);
    ofs << dest;
    ofs.close();
  }

  return fmt::format("{}image/{}/{}/{}.{}", url.empty() ? baseUrl : url, id,
                     genre, hash, SAVE_FORMAT);
}

string ImagesManager::getLocalPath(const string &id, const string &genre,
                                   const string &dest, const string &baseUrl) {
  filesystem::create_directories(fmt::format("../image/{}/{}", id, genre));

  return fmt::format("{}image/{}/{}/{}.{}", url.empty() ? baseUrl : url, id,
                     genre, dest, SAVE_FORMAT);
}

vector<string> ImagesManager::getImage(const string &id, const string &genre,
                                       const string &hash, bool asBase64) {

  string hashWithoutExtension = string(hash);
  RE2::GlobalReplace(&hashWithoutExtension, fmt::format(".{}", SAVE_FORMAT),
                     "");

  string path =
      fmt::format("../image/{}/{}/{}.src", id, genre, hashWithoutExtension);
  if (!filesystem::exists(path))
    throw "Image cannot be found";

  // check the cache
  string imagePath =
      driversManager.cmsId != nullptr && id == *driversManager.cmsId
          ? path
          : fmt::format("../image/{}/{}/{}.{}", id, genre, hashWithoutExtension,
                        SAVE_FORMAT);
  if (filesystem::exists(imagePath)) {
    std::ifstream imageFile(imagePath, std::ios::binary);

    std::ostringstream oss;
    oss << imageFile.rdbuf();
    imageFile.close();
    string imageData = oss.str();

    if (asBase64)
      return {"txt",
              fmt::format("data:{};base64, {}", mime_types.at(SAVE_FORMAT),
                          base64::to_base64(imageData))};

    return {SAVE_FORMAT, imageData};
  }

  std::ifstream ifs(path);
  string url;
  getline(ifs, url);

  RE2::GlobalReplace(&url, " ", "%20");

  // fetch the image
  cpr::Session session;
  session.SetUrl(cpr::Url(url));
  session.SetHeader(settings[id]);
  session.SetTimeout(cpr::Timeout{5000});
  session.SetHttpVersion(cpr::HttpVersion{
      cpr::HttpVersionCode::VERSION_2_0_PRIOR_KNOWLEDGE}); // Is this helping?

  if (this->proxy != nullptr)
    session.SetProxies(
        cpr::Proxies{{"https", *this->proxy}, {"http", *this->proxy}});

  cpr::Response r = session.Get();

  if (r.status_code >= 300 || r.status_code < 200)
    throw "Error fetching image";

  if (r.status_code == 0)
    throw "Request timeout";

  // retry if the content length is not matching
  if (r.header.find("content-length") != r.header.end() &&
      r.header.at("content-length") != to_string(r.downloaded_bytes))
    return this->getImage(id, genre, hash, asBase64);

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

  return this->getImage(id, genre, hash, asBase64);
}

string ImagesManager::saveImage(const string &id, const string &genre,
                                const string &image) {
  filesystem::create_directories(fmt::format("../image/{}/{}", id, genre));

  // generate the hash
  string hash = MD5()(image);
  string imagePath = fmt::format("../image/{}/{}/{}.src", id, genre, hash);
  if (filesystem::exists(imagePath))
    return hash;

  bool failed = false;
  FIMEMORY *hmem = FreeImage_OpenMemory((BYTE *)&image[0], image.length());

  try {
    FREE_IMAGE_FORMAT fif = FreeImage_GetFileTypeFromMemory(hmem, 0);

    if (fif == FIF_FORMAT) {
      fstream file(imagePath, std::ios::out | ios::binary);
      file.write(image.c_str(), image.size());
      file.close();
    } else {
      FIBITMAP *dib;
      FIBITMAP *converted_dib;

      try {
        dib = FreeImage_LoadFromMemory(fif, hmem);

        // check if the image was loaded successfully
        if (dib == nullptr)
          throw "Failed to load image";

        // save the image
        converted_dib = FreeImage_ConvertTo24Bits(dib);
        if (!FreeImage_Save(FIF_FORMAT, converted_dib, imagePath.c_str()))
          throw "Failed to save image";
      } catch (...) {
        failed = true;
      }

      FreeImage_Unload(converted_dib);
      FreeImage_Unload(dib);
    }
  } catch (...) {
    failed = true;
  }

  FreeImage_CloseMemory(hmem);

  if (failed)
    throw "Failed to save image";

  return hash;
}

void ImagesManager::deleteImage(const string &id, const string &genre,
                                const string &hash) {
  string imagePath = fmt::format("../image/{}/{}/{}.src", id, genre, hash);
  filesystem::remove(imagePath);
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