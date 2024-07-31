#include "accessGuard.hpp"
#include "../utils/utils.hpp"

#include <filesystem>
#include <iostream>
#include <thread>

#define CREATE_TOKEN_TABLES_SQL                                                \
  R"(CREATE TABLE IF NOT EXISTS "TOKEN"("ID" VARCHAR(255) NOT NULL PRIMARY KEY, "TOKEN" VARCHAR(255) NOT NULL);)"

#define TOKEN_LENGTH 128

void AccessGuard::applyConfig(json config) {
  if (config.contains("mode"))
    mode = config["mode"].get<string>();

  if (config.contains("key"))
    key = new string(config["key"].get<string>());

  if (config.contains("sql"))
    sqlName = new string(config["sql"].get<string>());

  if (config.contains("parameters"))
    parameters = new string(config["parameters"].get<string>());

  json admin;
  if (config.contains("admin"))
    admin = config["admin"];

  if (admin.contains("key"))
    adminKey = new string(admin["key"].get<string>());

  if (admin.contains("allowOnlyLocal"))
    allowOnlyLocal = admin["allowOnlyLocal"].get<bool>();

  if (mode == "token")
    initializeDatabase();
}

bool AccessGuard::verifyKey(string key) {
  bool result = true;

  if (mode == "key")
    result = this->key == nullptr || key == *this->key;

  if (mode == "token") {
    session sql(*pool);
    int count;

    sql << "SELECT COUNT(*) FROM TOKEN WHERE TOKEN = :token", use(key),
        into(count);

    result = count > 0;
  }

  return result;
}

bool AccessGuard::verifyAdminKey(string key, string ip) {
  return (!allowOnlyLocal || isLocalIp(ip)) &&
         (adminKey == nullptr || key == *adminKey);
}

map<string, string> AccessGuard::getToken(string id, int page) {
  session sql(*pool);

  rowset<row> rs =
      (sql.prepare
           << "SELECT * FROM TOKEN WHERE ID LIKE :id LIMIT 50 OFFSET :offest",
       use("%" + id + "%"), use((page - 1) * 50));

  map<string, string> result;
  for (auto it = rs.begin(); it != rs.end(); it++) {
    const row &row = *it;
    result[row.get<string>("ID")] = row.get<string>("TOKEN");
  }

  return result;
}

string AccessGuard::createToken(string id) {
  session sql(*pool);
  string token = randomString(TOKEN_LENGTH);

  sql << "INSERT INTO TOKEN (ID, TOKEN) VALUES (:id, :token)", use(id),
      use(token);

  return token;
}

void AccessGuard::removeToken(string id) {
  session sql(*pool);

  sql << "DELETE FROM TOKEN WHERE ID = :id", use(id);
}

string AccessGuard::refreshToken(string id) {
  session sql(*pool);
  string token = randomString(TOKEN_LENGTH);

  statement st =
      (sql.prepare << "UPDATE TOKEN SET TOKEN = :token WHERE ID = :id",
       use(token), use(id));
  st.execute(true);

  if (!st.get_affected_rows())
    throw "Not Updated";

  return token;
}

void AccessGuard::initializeDatabase() {
  // Connect to database
  try {
    int poolSize = thread::hardware_concurrency();
    pool = new connection_pool(poolSize);

    if (sqlName == nullptr)
      sqlName = new string("sqlite3");

    if (parameters == nullptr)
      parameters = new string(filesystem::absolute("../data/token.sqlite3"));

    for (size_t i = 0; i != poolSize; ++i) {
      session &sql = pool->at(i);
      sql.open(*sqlName, *parameters);
    }

    session sql(*pool);
    sql << CREATE_TOKEN_TABLES_SQL;

  } catch (string e) {
    log("AccessGuard", "Failed to Connect to the Database");
  }
}

AccessGuard accessGuard;