#include "crow.h"

#include "../manager/DriversManager.hpp"
#include "../models/ActiveDriver.hpp"
#include "../models/BaseDriver.hpp"
#include "../models/Manga.hpp"
#include "../utils/utils.hpp"

#include <SQLiteCpp/SQLiteCpp.h>
#include <algorithm>
#include <chrono>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <fstream>
#include <mutex>
#include <re2/re2.h>
#include <thread>

#define CHECK_ONLINE()                                                         \
  if (!isOnline)                                                               \
    throw "Database is offline";

using namespace std;

class ActiveAdapter : public BaseDriver {
public:
  ActiveAdapter(ActiveDriver *driver) : driver(driver) {
    id = driver->id;
    proxyHeaders = driver->proxyHeaders;
    supportedCategories = driver->supportedCategories;
    version = driver->version;
    supportSuggestion = true;

    // Connect to database
    try {
      db = new SQLite::Database("../data/" + id + ".sqlite3",
                                SQLite::OPEN_NOMUTEX | SQLite::OPEN_READWRITE);
    } catch (...) {
      isOnline = false;
    }

    // read the state
    ifstream inFile("../data/" + id + ".json");
    if (inFile.is_open()) {
      try {
        json state;
        inFile >> state;
        waitingList = state["waitingList"].get<vector<string>>();
      } catch (...) {
      }
    }
    inFile.close();

    ifstream ifs("../config.json");
    json config;
    ifs >> config;
    if (config.contains("proxies"))
      proxies = config["proxies"].get<vector<string>>();

    // start a thread
    thread(&ActiveAdapter::updater, this).detach();
  }

  vector<Manga *> getManga(vector<string> ids, bool showDetails) override {
    CHECK_ONLINE()

    ostringstream oss;
    oss << "'";
    for (size_t i = 0; i < ids.size(); ++i) {
      oss << ids[i];
      if (i < ids.size() - 1)
        oss << "', '";
    }
    oss << "'";

    SQLite::Statement query(*db, "SELECT * FROM MANGA M JOIN CHAPTERS C ON "
                                 "C.MANGA_ID = M.ID WHERE ID in (" +
                                     oss.str() + ")");

    map<string, Manga *> resultMap;
    while (query.executeStep()) {
      Manga *manga = toManga(query, showDetails);
      resultMap[manga->id] = manga;
    }

    if (showDetails) {
      // Get the chapters
      SQLite::Statement chapterQuery(
          *db, "SELECT * FROM CHAPTER WHERE CHAPTERS_ID in (" + oss.str() +
                   ") ORDER BY CHAPTERS_ID, -IDX");

      while (chapterQuery.executeStep()) {
        Chapter chapter = {chapterQuery.getColumn(3).getString(),
                           chapterQuery.getColumn(1).getString()};

        if (chapterQuery.getColumn(4).getInt() == 1)
          ((DetailsManga *)resultMap[chapterQuery.getColumn(0).getString()])
              ->chapters.extra.push_back(chapter);
        else
          ((DetailsManga *)resultMap[chapterQuery.getColumn(0).getString()])
              ->chapters.serial.push_back(chapter);
      }
    }

    vector<Manga *> result;
    // assign it to the manga
    for (auto const &pair : resultMap)
      result.push_back(pair.second);

    return result;
  }

  vector<string> getChapter(string id, string extraData) override {
    CHECK_ONLINE()

    string proxy;
    if (proxies.size() >= 2) {
      if (proxyCount < 1 || proxyCount >= proxies.size())
        proxyCount = 1;

      proxy = proxies.at(proxyCount);
      unique_lock<std::mutex> guard(mutex);
      proxyCount++;
      guard.unlock();
    }

    SQLite::Statement query(*db, "SELECT * FROM CHAPTER WHERE ID = ?");
    query.bind(1, id);
    query.executeStep();

    if (query.getColumn(5).isNull()) {
      vector<string> result = driver->getChapter(id, extraData, proxy);

      SQLite::Statement updateQuery(*db,
                                    "UPDATE CHAPTER SET URLS = ? WHERE ID = ? "
                                    "AND CHAPTERS_ID = ? AND IS_EXTRA = ?");
      updateQuery.bind(1, fmt::format("{}", fmt::join(result, "|")));
      updateQuery.bind(2, id);
      updateQuery.bind(3, query.getColumn(0).getString());
      updateQuery.bind(4, query.getColumn(4).getString());
      updateQuery.exec();

      return result;
    } else {
      return split(query.getColumn(5).getString(), R"(\|)");
    }
  }

  vector<Manga *> getList(Category category, int page, Status status) override {
    CHECK_ONLINE()

    string queryString = "SELECT * FROM MANGA ";
    if (status != Any)
      queryString += " WHERE IS_END = " + to_string(status == Ended);
    queryString += " ORDER BY -UPDATE_TIME LIMIT 50 OFFSET ?";

    SQLite::Statement query(*db, queryString);
    query.bind(1, (page - 1) * 50);

    vector<Manga *> result;
    while (query.executeStep())
      result.push_back(toManga(query));

    return result;
  }

  vector<string> getSuggestion(string keyword) override {
    CHECK_ONLINE()

    SQLite::Statement query(*db,
                            "SELECT * FROM MANGA WHERE TITLE LIKE ? LIMIT 5");
    query.bind(1, "%" + keyword + "%");

    vector<string> result;
    while (query.executeStep())
      result.push_back(query.getColumn(2).getString());

    return result;
  }

  vector<Manga *> search(string keyword, int page) override {
    CHECK_ONLINE()

    SQLite::Statement query(
        *db, "SELECT * FROM MANGA WHERE TITLE LIKE ? LIMIT 50 OFFSET ?");
    query.bind(1, "%" + keyword + "%");
    query.bind(2, (page - 1) * 50);

    vector<Manga *> result;
    while (query.executeStep())
      result.push_back(toManga(query));

    return result;
  }

  bool checkOnline() override { return this->isOnline; }

  ~ActiveAdapter() {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"
    delete driver;
#pragma GCC diagnostic pop

    delete db;
  }

private:
  ActiveDriver *driver;
  SQLite::Database *db;
  bool isOnline = true;
  int counter = 300;
  vector<string> waitingList;
  vector<string> proxies;
  int proxyCount = 0;
  std::mutex mutex;

  Manga *toManga(SQLite::Statement &query, bool showDetails = false) {
    if (showDetails) {
      vector<string> categoriesString =
          split(query.getColumn(6).getString(), R"(\|)");
      vector<Category> categories;
      for (string category : categoriesString)
        categories.push_back(stringToCategory(category));
      int *updateTime = new int(query.getColumn(8).getInt());

      return new DetailsManga(
          this, query.getColumn(0).getString(), query.getColumn(2).getString(),
          query.getColumn(1).getString(), query.getColumn(7).getString(),
          split(query.getColumn(5).getString(), R"(\|)"),
          query.getColumn(4).getInt() == 1, query.getColumn(3).getString(),
          categories, {{}, {}, query.getColumn(10).getString()}, updateTime);
    } else {
      return new Manga(
          this, query.getColumn(0).getString(), query.getColumn(2).getString(),
          query.getColumn(1).getString(), query.getColumn(7).getString(),
          query.getColumn(4).getInt() == 1);
    }
  }

  void updater() {
    if (!isOnline)
      return;

    while (!driversManager.isReady)
      this_thread::sleep_for(chrono::milliseconds(100));

    if (driversManager.get(this->id) == nullptr)
      return;

    string proxy;
    if (proxies.size() >= 1)
      proxy = proxies.at(0);

    json state;
    string id;

    while (true) {
      // check if any updates every 5 minutes
      if (counter >= 300) {
        vector<PreviewManga> manga;
        try {
          CROW_LOG_INFO << fmt::format("ActiveDriver - {}: getting updates",
                                       this->id);
          manga = driver->getUpdates(proxy);
        } catch (...) {
          CROW_LOG_INFO << fmt::format(
              "ActiveDriver - {}: failed to get updates", this->id);
          continue;
        }

        map<string, string> latestMap;
        ostringstream oss;
        oss << "'";
        for (size_t i = 0; i < manga.size(); ++i) {
          oss << manga[i].id;
          latestMap[manga[i].id] = manga[i].latest;
          if (i < manga.size() - 1)
            oss << "', '";
        }
        oss << "'";

        SQLite::Statement query(
            *db,
            fmt::format("SELECT * FROM MANGA WHERE ID in ({})", oss.str()));
        while (query.executeStep()) {
          id = query.getColumn(0).getString();

          // check if updated and not in the waiting list

          if (!this->driver->isLatestEqual(query.getColumn(7).getString(),
                                           latestMap[id]) &&
              find(waitingList.begin(), waitingList.end(), id) ==
                  waitingList.end())
            waitingList.insert(waitingList.begin(), id);

          latestMap.erase(id);
        }

        // if not found in database
        for (const auto &pair : latestMap)
          waitingList.insert(waitingList.begin(), pair.first);

        // reset counter
        counter = 0;
      } else if (!waitingList.empty()) {
        id = waitingList.back();
        waitingList.pop_back();
        try {
          CROW_LOG_INFO << fmt::format("ActiveDriver - {}: getting {}",
                                       this->id, id);
          DetailsManga *manga =
              (DetailsManga *)driver->getManga({id}, true, proxy).at(0);

          ostringstream categories;
          for (size_t i = 0; i < manga->categories.size(); ++i) {
            categories << categoryToString(manga->categories[i]);
            if (i < manga->categories.size() - 1)
              categories << "|";
          }

          SQLite::Transaction transaction(*db);

          // update the manga info
          SQLite::Statement query(
              *db, "REPLACE INTO MANGA (ID, THUMBNAIL, TITLE, DESCRIPTION, "
                   "IS_END, AUTHORS, CATEGORIES, LATEST, UPDATE_TIME) VALUES "
                   "(?, ?, ?, ?, ?, ?, ?, ?, ?)");
          query.bind(1, manga->id);
          query.bind(2, manga->thumbnail);
          query.bind(3, manga->title);
          query.bind(4, manga->description);
          query.bind(5, (int)manga->isEnded);
          query.bind(6, fmt::format("{}", fmt::join(manga->authors, "|")));
          query.bind(7, categories.str());
          query.bind(8, manga->latest);
          query.bind(9, chrono::duration_cast<chrono::seconds>(
                            chrono::system_clock::now().time_since_epoch())
                            .count());
          query.exec();

          query = SQLite::Statement(
              *db,
              "REPLACE INTO CHAPTERS (MANGA_ID, EXTRA_DATA) VALUES (?, ?)");
          query.bind(1, manga->id);
          query.bind(2, manga->chapters.extraData);
          query.exec();

          auto updateChapter = [&](vector<Chapter> chapters, bool isExtra) {
            ostringstream oss;
            map<string, int> chaptersMap;
            oss << "'";
            for (size_t i = 0; i < chapters.size(); ++i) {
              oss << chapters[i].id;
              chaptersMap[chapters[i].id] = i;
              if (i < chapters.size() - 1)
                oss << "', '";
            }
            oss << "'";

            // remove the deleted chapter
            query = SQLite::Statement(
                *db, fmt::format("DELETE FROM CHAPTER WHERE CHAPTERS_ID = ? "
                                 "AND IS_EXTRA = ? AND ID NOT IN ({})",
                                 oss.str()));
            query.bind(1, manga->id);
            query.bind(2, (int)isExtra);
            query.exec();

            // insert only the new one
            SQLite::Statement query(*db, "SELECT ID FROM CHAPTER WHERE "
                                         "IS_EXTRA = ? AND CHAPTERS_ID = ?");
            query.bind(1, (int)isExtra);
            query.bind(2, manga->id);

            while (query.executeStep())
              chaptersMap.erase(query.getColumn(0).getString());

            for (const auto &pair : chaptersMap) {
              Chapter chapter = chapters[pair.second];
              query = SQLite::Statement(
                  *db, "REPLACE INTO CHAPTER (CHAPTERS_ID, ID, IDX, TITLE, "
                       "IS_EXTRA) VALUES (?, ?, ?, ?, ?)");
              query.bind(1, manga->id);
              query.bind(2, chapter.id);
              query.bind(3, (int)(chapters.size() - pair.second - 1));
              query.bind(4, chapter.title);
              query.bind(5, (int)isExtra);
              query.exec();
            }
          };

          updateChapter(manga->chapters.serial, false);
          updateChapter(manga->chapters.extra, true);

          transaction.commit();
        } catch (...) {
          CROW_LOG_INFO << fmt::format(
              "ActiveDriver - {}: failed to getting {}", this->id, id);
          waitingList.insert(waitingList.begin(), id);
        }
      }

      // save state
      state["waitingList"] = waitingList;
      ofstream outFile("../data/" + this->id + ".json");
      outFile << state;
      outFile.close();

      counter += driver->timeout;
      this_thread::sleep_for(chrono::seconds(driver->timeout));
    }
  }
};