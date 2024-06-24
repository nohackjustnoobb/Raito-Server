#pragma once

#include "../models/baseDriver.hpp"

using namespace std;

// This class is used to manage the drivers.
class DriversManager {
public:
  // Determine if the drivers should be started.
  bool isReady = false;
  // The id of the CMS driver
  string *cmsId;

  // Add the driver to the manager.
  void add(BaseDriver *driver);

  // Get the driver from the manager with the given id (Case insensitive).
  BaseDriver *get(string id);
  BaseDriver *operator[](string id);

  // Get all the registered drivers.
  vector<BaseDriver *> getAll();

private:
  vector<BaseDriver *> drivers;
};

extern DriversManager driversManager;