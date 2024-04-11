#include "DriversManager.hpp"
#include "ImagesManager.hpp"

#include <algorithm>

void DriversManager::add(BaseDriver *driver) { drivers.push_back(driver); }

BaseDriver *DriversManager::get(string id) {
  for (const auto &driver : drivers) {
    if (driver->id == id)
      return driver;
  }

  return nullptr;
}

BaseDriver *DriversManager::operator[](string id) { return get(id); }

vector<BaseDriver *> DriversManager::getAll() { return drivers; }

DriversManager driversManager;