#include "manager/accessGuard.hpp"
#include "manager/driversManager.hpp"
#include "manager/imagesManager.hpp"
#include "utils/log.hpp"
#include "utils/utils.hpp"

// Drivers
#include "drivers/activeAdapter/activeAdapter.hpp"
#include "drivers/dm5/dm5.hpp"
#include "drivers/mhg/mhg.hpp"
#include "drivers/mhr/mhr.hpp"
#include "drivers/selfContained/selfContained.hpp"

// Backend servers
#include "server/drogon.hpp"

#include <FreeImage.h>
#include <soci/sqlite3/soci-sqlite3.h>
// #include <soci/mysql/soci-mysql.h>
// #include <soci/postgresql/soci-postgresql.h>

#define RAITO_SERVER_VERSION "1.0.0-beta.40"

// Main entry point
int main() {
  log("RaitoServer",
      fmt::format("Running at Version {}", RAITO_SERVER_VERSION));
  setenv("RAITO_SERVER_VERSION", RAITO_SERVER_VERSION, 1);

  string *webpageUrl = nullptr;
  json accessGuardOption;
  bool adminAllowOnlyLocal = true;
  int port = 8000;

  // TODO You can add your own drivers here:
  BaseDriver *drivers[] = {
      new MHR(),
      new DM5(),
      new ActiveAdapter(new MHG()),
  };

  // Initialize the libraries
  FreeImage_Initialise();
  soci::register_factory_sqlite3();
  // FIXME dont know why this is not working
  // soci::register_factory_mysql();
  // soci::register_factory_postgresql();

  auto applyConfig = [&](json config) {
    if (config.contains("server")) {
      json server = config["server"];
      // initialize the imagesManager
      if (server.contains("baseUrl")) {
        string baseUrl = server["baseUrl"].get<string>();
        imagesManager.setBaseUrl(baseUrl);
      }

      // set port
      if (server.contains("port"))
        port = server["port"].get<int>();

      // set the webpage url
      if (server.contains("webpageUrl")) {
        string url = server["webpageUrl"].get<string>();

        webpageUrl = new string(url);

      } else {
        log("RaitoServer", "\"webpageUrl\" is not found in the configuration "
                           "file; some features will be disabled");
      }
    }

    if (config.contains("driver")) {
      json driver = config["driver"];

      // register drivers
      if (driver.contains("include")) {
        vector<string> include = driver["include"].get<vector<string>>();

        for (const auto &driver : drivers)
          if (find(include.begin(), include.end(), driver->id) != include.end())
            driversManager.add(driver);

      } else if (driver.contains("exclude")) {
        vector<string> exclude = driver["exclude"].get<vector<string>>();

        for (const auto &driver : drivers)
          if (std::find(exclude.begin(), exclude.end(), driver->id) ==
              exclude.end())
            driversManager.add(driver);
      } else {
        for (const auto &driver : drivers)
          driversManager.add(driver);
      }

      for (auto &driverObject : driversManager.getAll()) {
        json driverConfig = {};

        if (driver.contains("config") &&
            driver["config"].contains(driverObject->id))
          driverConfig = driver["config"][driverObject->id];

        if (driver.contains("proxies"))
          driverConfig["proxies"] = driver["proxies"];

        driverObject->applyConfig(driverConfig);
      }
    }

    if (config.contains("image")) {
      json image = config["image"];
      // set image proxy
      if (image.contains("proxy"))
        imagesManager.setProxy(image["proxy"].get<string>());

      // set interval
      if (image.contains("clearCaches"))
        imagesManager.setInterval(image["clearCaches"].get<string>());
    }

    if (config.contains("accessGuard"))
      accessGuardOption = config["accessGuard"];

    if (config.contains("CMS")) {
      json cms = config["CMS"];
      if (cms.contains("enabled") && cms["enabled"].get<bool>()) {
        BaseDriver *driver = new SelfContained();
        driver->applyConfig(cms);
        driversManager.add(driver);
        driversManager.cmsId = new string(driver->id);
      }

      accessGuard.applyConfig(accessGuardOption);
    }
  };

  // Read the configuration file
  ifstream ifs("../config.json");
  if (ifs.is_open()) {
    json config;
    ifs >> config;
    applyConfig(config);
    ifs.close();
  } else {
    log("RaitoServer",
        "Configuration file not found, some features will be disabled");
    for (const auto &driver : drivers)
      driversManager.add(driver);
  }

  // All libraries are initialized
  driversManager.isReady = true;

  startDrogonServer(port, webpageUrl);

  FreeImage_DeInitialise();

  return 0;
}