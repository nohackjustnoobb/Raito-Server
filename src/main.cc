#include "manager/DriversManager.hpp"
#include "manager/ImagesManager.hpp"

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

#define RAITO_SERVER_VERSION "0.1.0-beta.18"

int main() {
  setenv("RAITO_SERVER_VERSION", RAITO_SERVER_VERSION, 1);

  // read the configuration file
  ifstream ifs("../config.json");
  if (!ifs.is_open()) {
    log("Main", "Configuration file not found");
    return 1;
  }

  json config;
  ifs >> config;
  if (!config.contains("baseUrl")) {
    log("Main", "\"baseUrl\" not found in the configuration file");
    return 1;
  }

  // initialize the imagesManager
  string baseUrl = config["baseUrl"].get<string>();
  if (!RE2::FullMatch(baseUrl, R"(^https?:\/\/.*\/$)"))
    throw "\"baseUrl\" is not valid";

  imagesManager.setBaseUrl(baseUrl);

  string *webpageUrl = nullptr;
  // set the webpage url
  if (config.contains("webpageUrl")) {
    string url = config["webpageUrl"].get<string>();

    if (RE2::FullMatch(url, R"(^https?:\/\/.*\/$)"))
      webpageUrl = new string(url);
    else
      log("Main",
          "\"webpageUrl\" is not valid, some features will be disabled");

  } else {
    log("Main",
        "\"webpageUrl\" not found in the configuration file, some features "
        "will be disabled");
  }

  string *accessKey = nullptr;
  if (config.contains("accessKey") && !config["accessKey"].is_null() &&
      config["accessKey"].get<string>() != "")
    accessKey = new string(config["accessKey"].get<string>());

  // set port
  int port = 8000;
  if (config.contains("port"))
    port = config["port"].get<int>();
  setenv("RAITO_SERVER_PORT", to_string(port).c_str(), 1);

  // set image proxy
  if (config.contains("imageProxy"))
    imagesManager.setProxy(config["imageProxy"].get<string>());

  // set interval
  if (config.contains("clearCache"))
    imagesManager.setInterval(config["clearCache"].get<string>());

  // TODO You can add your own drivers here:
  BaseDriver *drivers[] = {
      new MHR(),
      new DM5(),
      new ActiveAdapter(new MHG()),
  };

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
      if (std::find(excludeDrivers.begin(), excludeDrivers.end(), driver->id) ==
          excludeDrivers.end())
        driversManager.add(driver);
  } else {
    for (const auto &driver : drivers)
      driversManager.add(driver);
  }
  driversManager.isReady = true;

  // server initialization
  FreeImage_Initialise();

  if (config.contains("framework") &&
      config["framework"].get<string>() == "drogon")
    startDrogonServer(webpageUrl, accessKey);
  else
    startCrowServer(webpageUrl, accessKey);

  FreeImage_DeInitialise();

  return 0;
}