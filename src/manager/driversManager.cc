#include "driversManager.hpp"
#include "../utils/log.hpp"
#include "../utils/utils.hpp"
#include "imagesManager.hpp"

void DriversManager::add(BaseDriver *driver) {
  drivers.push_back(driver);
  imagesManager.add(driver->id, driver->proxyHeaders);
  log("DriversManager", "Registered " + driver->id);
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