#include "DriversManager.hpp"
#include "ImagesManager.hpp"

#include <algorithm>

void DriversManager::add(BaseDriver *driver) {
  drivers.push_back(driver);
  imagesManager.add(driver->id, driver->proxyHeaders);
}

BaseDriver *DriversManager::get(string id) {
  // the id should not be case sensitive
  transform(id.begin(), id.end(), id.begin(),
            [](unsigned char c) { return toupper(c); });

  for (const auto &driver : drivers) {
    if (driver->id == id)
      return driver;
  }

  return nullptr;
}

BaseDriver *DriversManager::operator[](string id) { return get(id); }

vector<BaseDriver *> DriversManager::getAll() { return drivers; }

DriversManager driversManager;