#include "DriversManager.hpp"
#include "ImagesManager.hpp"

#include <algorithm>

void DriversManager::add(BaseDriver *driver) {
  drivers[driver->id] = driver;
  imagesManager.add(driver->id, driver->proxyHeaders);
}

BaseDriver *DriversManager::get(string id) {
  // the id should not be case sensitive
  transform(id.begin(), id.end(), id.begin(),
            [](unsigned char c) { return toupper(c); });

  return drivers.find(id) == drivers.end() ? nullptr : drivers[id];
}

vector<BaseDriver *> DriversManager::getAll() {
  vector<BaseDriver *> result;

  for (const auto &pair : drivers)
    result.push_back(pair.second);

  return result;
}

DriversManager driversManager;