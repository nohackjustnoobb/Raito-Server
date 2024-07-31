#pragma once

#include <map>
#include <nlohmann/json.hpp>
#include <soci/soci.h>
#include <string>

using json = nlohmann::json;
using namespace std;
using namespace soci;

class AccessGuard {
public:
  // Options: "none", "key", "token"
  string mode = "none";

  void applyConfig(json config);

  bool verifyKey(string key);

  bool verifyAdminKey(string key, string ip);

  map<string, string> getToken(string id, int page);

  string createToken(string id);

  void removeToken(string id);

  string refreshToken(string id);

private:
  connection_pool *pool;
  // For "key" mode only
  string *key;
  // For "token" mode only
  string *sqlName;
  string *parameters;
  // For "admin"
  string *adminKey;
  bool allowOnlyLocal = true;

  void initializeDatabase();
};

extern AccessGuard accessGuard;