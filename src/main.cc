#include "manager/DriversManager.hpp"
#include "manager/ImagesManager.hpp"
#include "utils/utils.hpp"

// Drivers
#include "drivers/ActiveAdapter.cc"
#include "drivers/DM5/DM5.hpp"
#include "drivers/MHG/MHG.hpp"
#include "drivers/MHR/MHR.hpp"

// Backend servers
#include "server/crow.hpp"
#include "server/drogon.hpp"

#include <FreeImage.h>
#include <cstdlib>
#include <re2/re2.h>
#include <string>

#define RAITO_SERVER_VERSION "0.1.0-beta.20"
#define RAITO_DEFAULT_FRAMEWORK "crow"

int main() {
  log("RaitoServer",
      fmt::format("Running at Version {}", RAITO_SERVER_VERSION));
  setenv("RAITO_SERVER_VERSION", RAITO_SERVER_VERSION, 1);

  string *webpageUrl = nullptr;
  string *accessKey = nullptr;
  string framework = RAITO_DEFAULT_FRAMEWORK;
  int port = 8000;

  // TODO You can add your own drivers here:
  BaseDriver *drivers[] = {
      new MHR(),
      new DM5(),
      new ActiveAdapter(new MHG()),
  };

  auto applyConfig = [&](json config) {
    // initialize the imagesManager
    if (config.contains("baseUrl")) {
      string baseUrl = config["baseUrl"].get<string>();
      if (RE2::FullMatch(baseUrl, R"(^https?:\/\/.*\/$)"))
        imagesManager.setBaseUrl(baseUrl);
      else
        log("RaitoServer", "\"baseUrl\" is not valid and is ignored");
    }

    // set the webpage url
    if (config.contains("webpageUrl")) {
      string url = config["webpageUrl"].get<string>();

      if (RE2::FullMatch(url, R"(^https?:\/\/.*\/$)"))
        webpageUrl = new string(url);
      else
        log("RaitoServer", "\"webpageUrl\" is not valid and is ignored; some "
                           "features will be disabled");
    } else {
      log("RaitoServer", "\"webpageUrl\" is not found in the configuration "
                         "file; some features will be disabled");
    }

    // set the access key
    if (config.contains("accessKey") && !config["accessKey"].is_null() &&
        config["accessKey"].get<string>() != "")
      accessKey = new string(config["accessKey"].get<string>());

    // set port
    if (config.contains("port"))
      port = config["port"].get<int>();

    // set image proxy
    if (config.contains("imageProxy"))
      imagesManager.setProxy(config["imageProxy"].get<string>());

    // set interval
    if (config.contains("clearCache"))
      imagesManager.setInterval(config["clearCache"].get<string>());

    // register drivers
    if (config.contains("includeDrivers")) {
      vector<string> includeDrivers =
          config["includeDrivers"].get<vector<string>>();

      for (const auto &driver : drivers)
        if (find(includeDrivers.begin(), includeDrivers.end(), driver->id) !=
            includeDrivers.end())
          driversManager.add(driver);

    } else if (config.contains("excludeDrivers")) {
      vector<string> excludeDrivers =
          config["excludeDrivers"].get<vector<string>>();

      for (const auto &driver : drivers)
        if (std::find(excludeDrivers.begin(), excludeDrivers.end(),
                      driver->id) == excludeDrivers.end())
          driversManager.add(driver);
    } else {
      for (const auto &driver : drivers)
        driversManager.add(driver);
    }

    if (config.contains("framework"))
      framework = config["framework"].get<string>();
  };

  // read the configuration file
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
  driversManager.isReady = true;

  // server initialization
  FreeImage_Initialise();

  if (framework == "drogon")
    startDrogonServer(port, webpageUrl, accessKey);
  else
    startCrowServer(port, webpageUrl, accessKey);

  FreeImage_DeInitialise();

  return 0;
}