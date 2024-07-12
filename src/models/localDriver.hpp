#pragma once

#include "../manager/driversManager.hpp"
#include "../utils/utils.hpp"
#include "baseDriver.hpp"
#include "manga.hpp"

#include <rapidfuzz/fuzz.hpp>
#include <soci/soci.h>

using namespace std;
using namespace soci;

#define CHECK_ONLINE()                                                         \
  if (!isOnline)                                                               \
    throw "Database is offline";

#define CREATE_MANGA_TABLES_SQL                                                \
  R"(CREATE TABLE IF NOT EXISTS "MANGA"("ID" VARCHAR(255) NOT NULL PRIMARY KEY, "AUTHORS" VARCHAR(255) NOT NULL, "CATEGORIES" VARCHAR(255) NOT NULL, "DESCRIPTION" TEXT NOT NULL, "IS_ENDED" INTEGER NOT NULL, "LATEST" VARCHAR(255) NOT NULL, "THUMBNAIL" VARCHAR(255) NOT NULL, "TITLE" VARCHAR(255) NOT NULL, "UPDATE_TIME" INTEGER NOT NULL, "EXTRA_DATA" VARCHAR(255) NOT NULL);)"

#define CREATE_CHAPTER_TABLES_SQL                                              \
  R"(CREATE TABLE IF NOT EXISTS "CHAPTER" ("MANGA_ID" VARCHAR(255) NOT NULL, "ID" VARCHAR(255) NOT NULL, "IDX" INTEGER NOT NULL, "IS_EXTRA" INTEGER NOT NULL, "TITLE" VARCHAR(255) NOT NULL, "URLS" TEXT, PRIMARY KEY ("MANGA_ID", "ID"), FOREIGN KEY ("MANGA_ID") REFERENCES "MANGA" ("ID")); CREATE INDEX "chaptermodel_MANGA_ID" ON "CHAPTER" ("MANGA_ID");)"

class LocalDriver : public BaseDriver {
public:
  virtual vector<Manga *> getManga(vector<string> ids,
                                   bool showDetails) override {
    CHECK_ONLINE()

    ostringstream oss;
    oss << "'";
    for (size_t i = 0; i < ids.size(); ++i) {
      // prevent SQL injection
      if (RE2::FullMatch(ids[i],
                         R"(^[A-Za-z0-9\-_.~!#$&'()*+,\/:;=?@\[\]]*$)")) {
        oss << ids[i];
        if (i < ids.size() - 1)
          oss << "','";
      }
    }
    oss << "'";

    session sql(*pool);
    rowset<row> rs = sql.prepare << fmt::format(
                         "SELECT * FROM MANGA WHERE ID IN ({})", oss.str());

    map<string, Manga *> resultMap;
    for (auto it = rs.begin(); it != rs.end(); it++) {
      const row &row = *it;
      Manga *manga = toManga(row, showDetails);
      resultMap[manga->id] = manga;
    }

    if (showDetails) {
      // Get the chapters
      rs = sql.prepare << fmt::format("SELECT * FROM CHAPTER WHERE MANGA_ID IN "
                                      "({}) ORDER BY MANGA_ID, -IDX",
                                      oss.str());

      for (auto it = rs.begin(); it != rs.end(); it++) {
        const row &row = *it;

        Chapter chapter = {row.get<string>("TITLE"), row.get<string>("ID")};

        if (row.get<int>("IS_EXTRA") == 1)
          ((DetailsManga *)resultMap[row.get<string>("MANGA_ID")])
              ->chapters.extra.push_back(chapter);
        else
          ((DetailsManga *)resultMap[row.get<string>("MANGA_ID")])
              ->chapters.serial.push_back(chapter);
      }
    }

    vector<Manga *> result;
    // convert it to vector of manga
    for (auto const &id : ids)
      if (resultMap.count(id))
        result.push_back(resultMap[id]);

    return result;
  }

  virtual vector<string> getChapter(string id, string extraData) override {
    CHECK_ONLINE()

    session sql(*pool);
    string urls;
    indicator ind;
    sql << "SELECT URLS FROM CHAPTER WHERE ID = :id AND MANGA_ID = :manga_id",
        into(urls, ind), use(id), use(extraData);

    if (ind == i_null)
      return {};

    return split(urls, R"(\|)");
  }

  virtual vector<Manga *> getList(Category category, int page,
                                  Status status) override {
    CHECK_ONLINE()

    string queryString = "SELECT * FROM MANGA";
    if (status != Any)
      queryString += " WHERE IS_ENDED = " + to_string(status == Ended);
    if (category != All)
      queryString +=
          (status == Any ? " WHERE" : " AND") +
          (" CATEGORIES LIKE '%" + categoryToString(category) + "%'");
    queryString += " ORDER BY -UPDATE_TIME LIMIT 50 OFFSET :offset";

    session sql(*pool);
    rowset<row> rs = (sql.prepare << queryString, use((page - 1) * 50));

    vector<Manga *> result;
    for (auto it = rs.begin(); it != rs.end(); it++) {
      const row &row = *it;
      result.push_back(toManga(row));
    }

    return result;
  }

  virtual vector<string> getSuggestion(string keyword) override {
    CHECK_ONLINE()

    return extract(keyword);
  }

  virtual vector<Manga *> search(string keyword, int page) override {
    CHECK_ONLINE()

    vector<string> titlesRank = extract(keyword, page * 50);
    vector<string> resultTitles;
    int padding = (page - 1) * 50;
    if (titlesRank.size() >= padding)
      resultTitles = vector(titlesRank.begin() + padding, titlesRank.end());

    vector<string> resultId;
    for (const auto &title : resultTitles)
      resultId.insert(resultId.end(), titlesWithId[title].begin(),
                      titlesWithId[title].end());

    // remove duplicates
    sort(resultId.begin(), resultId.end());
    resultId.erase(unique(resultId.begin(), resultId.end()), resultId.end());

    return getManga(resultId, false);
  }

  virtual bool checkOnline() override { return isOnline; }

  virtual void applyConfig(json config) override {
    if (config.contains("parameters"))
      parameters = config["parameters"].get<string>();

    if (config.contains("sql"))
      sqlName = config["sql"].get<string>();
  }

protected:
  bool isOnline = true;
  connection_pool *pool;
  string parameters;
  string sqlName = "sqlite3";
  map<string, vector<string>> titlesWithId;
  vector<string> titles;

  void initializeDatabase() {
    while (!driversManager.isReady)
      this_thread::sleep_for(chrono::milliseconds(100));

    // Connect to database
    try {
      int poolSize = thread::hardware_concurrency();
      pool = new connection_pool(poolSize);

      if (parameters == "")
        parameters = filesystem::absolute("../data/" + id + ".sqlite3");

      for (size_t i = 0; i != poolSize; ++i) {
        session &sql = pool->at(i);
        sql.open(sqlName, parameters);
      }

      session sql(*pool);
      sql << CREATE_MANGA_TABLES_SQL;
      sql << CREATE_CHAPTER_TABLES_SQL;

      thread(&LocalDriver::titlesCacheUpdateLoop, this).detach();
    } catch (...) {
      isOnline = false;
    }

    if (!isOnline)
      log(id, "Failed to Connect to the Database");
  }

  Manga *toManga(const row &row, bool showDetails = false) {
    if (showDetails) {
      vector<Category> categories;
      for (string category : split(row.get<string>("CATEGORIES"), R"(\|)"))
        categories.push_back(stringToCategory(category));
      int *updateTime = new int(row.get<int>("UPDATE_TIME"));

      return new DetailsManga(
          this, row.get<string>("ID"), row.get<string>("TITLE"),
          row.get<string>("THUMBNAIL"), row.get<string>("LATEST"),
          split(row.get<string>("AUTHORS"), R"(\|)"),
          row.get<int>("IS_ENDED") == 1, row.get<string>("DESCRIPTION"),
          categories, {{}, {}, row.get<string>("EXTRA_DATA")}, updateTime);
    } else {
      return new Manga(this, row.get<string>("ID"), row.get<string>("TITLE"),
                       row.get<string>("THUMBNAIL"), row.get<string>("LATEST"),
                       row.get<int>("IS_ENDED") == 1);
    }
  }

  vector<string> extract(string keyword, int limit = 5) {
    vector<pair<double, string>> top_titles;
    rapidfuzz::fuzz::CachedRatio scorer(keyword);

    for (const auto &title : titles) {
      double score = scorer.similarity(title);

      if (top_titles.size() < limit) {
        top_titles.emplace_back(score, title);
        sort(top_titles.begin(), top_titles.end(), greater<>());
      } else if (score > top_titles.back().first) {
        top_titles.back() = make_pair(score, title);
        sort(top_titles.begin(), top_titles.end(), greater<>());
      }
    }

    vector<string> result;
    for (const auto &entry : top_titles)
      result.push_back(entry.second);

    return result;
  }

private:
  void updateCaches() {
    session sql(*pool);

    // update the cached titles
    rowset<row> titlesRs = sql.prepare << "SELECT ID, TITLE FROM MANGA";
    titles.clear();
    titlesWithId.clear();
    for (auto it = titlesRs.begin(); it != titlesRs.end(); it++) {
      const row &row = *it;
      titlesWithId[row.get<string>("TITLE")].push_back(row.get<string>("ID"));
      titles.push_back(row.get<string>("TITLE"));
    }
  }

  void titlesCacheUpdateLoop() {
    while (true) {
      if (isOnline)
        updateCaches();
      this_thread::sleep_for(chrono::minutes(5));
    }
  }
};