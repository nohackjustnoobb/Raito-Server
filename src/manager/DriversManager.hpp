#pragma once

#include "../models/BaseDriver.hpp"

using namespace std;

class DriversManager {
public:
  bool isReady = false;

  void add(BaseDriver *driver);

  BaseDriver *get(string id);

  BaseDriver *operator[](string id);

  vector<BaseDriver *> getAll();

private:
  vector<BaseDriver *> drivers;
};

extern DriversManager driversManager;