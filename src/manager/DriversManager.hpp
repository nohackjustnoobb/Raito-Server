#pragma once

#include "../models/BaseDriver.hpp"

using namespace std;

class DriversManager {
public:
  bool isReady = false;

  void add(BaseDriver *driver);

  BaseDriver *get(string id);

  vector<BaseDriver *> getAll();

private:
  map<string, BaseDriver *> drivers;
};

extern DriversManager driversManager;