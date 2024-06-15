#include "../../manager/DriversManager.hpp"
#include "../../models/ActiveDriver.hpp"
#include "../../models/BaseDriver.hpp"
#include "../../models/Manga.hpp"
#include "../../utils/utils.hpp"

#include <algorithm>
#include <chrono>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <fstream>
#include <mutex>
#include <re2/re2.h>
#include <soci/soci.h>
#include <thread>

#define CHECK_ONLINE()                                                         \
  if (!isOnline)                                                               \
    throw "Database is offline";

using namespace std;
using namespace soci;

class ActiveAdapter : public BaseDriver {
public:
  ActiveAdapter(ActiveDriver *driver) : driver(driver) {
    id = driver->id;
    proxyHeaders = driver->proxyHeaders;
    supportedCategories = driver->supportedCategories;
    version = driver->version;
    supportSuggestion = true;
    parameters = filesystem::absolute("../data/" + id + ".sqlite3");

    // start a thread
    thread(&ActiveAdapter::mainLoop, this).detach();
  }

  vector<Manga *> getManga(vector<string> ids, bool showDetails) override {
    CHECK_ONLINE()

    ostringstream oss;
    oss << "'";
    for (size_t i = 0; i < ids.size(); ++i) {
      oss << ids[i];
      if (i < ids.size() - 1)
        oss << "','";
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

    session sql(*pool);
    string urls;
    indicator ind;
    sql << "SELECT URLS FROM CHAPTER WHERE ID = :id AND MANGA_ID = :manga_id",
        into(urls, ind), use(id), use(extraData);

    if (ind == i_null) {
      vector<string> result = driver->getChapter(id, extraData, proxy);

      // TODO possibly SQL injection
      sql << fmt::format(
          "UPDATE CHAPTER SET URLS = '{}' WHERE ID = {} AND MANGA_ID = {}",
          fmt::join(result, "|"), id, extraData);

      return result;
    } else {
      return split(urls, R"(\|)");
    }
  }

  vector<Manga *> getList(Category category, int page, Status status) override {
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

  vector<string> getSuggestion(string keyword) override {
    CHECK_ONLINE()

    session sql(*pool);
    rowset<row> rs =
        (sql.prepare
             << "SELECT TITLE FROM MANGA WHERE TITLE LIKE :keyword LIMIT 5",
         use("%" + keyword + "%"));

    vector<string> result;
    for (auto it = rs.begin(); it != rs.end(); it++) {
      const row &row = *it;
      result.push_back(row.get<string>("TITLE"));
    }

    return result;
  }

  vector<Manga *> search(string keyword, int page) override {
    CHECK_ONLINE()

    session sql(*pool);
    rowset<row> rs = (sql.prepare << "SELECT * FROM MANGA WHERE TITLE LIKE "
                                     ":keyword LIMIT 50 OFFSET :offset",
                      use("%" + keyword + "%"), use((page - 1) * 50));

    vector<Manga *> result;
    for (auto it = rs.begin(); it != rs.end(); it++) {
      const row &row = *it;
      result.push_back(toManga(row));
    }

    return result;
  }

  bool checkOnline() override { return this->isOnline; }

  void applyConfig(json config) override {
    if (config.contains("proxies"))
      proxies = config["proxies"].get<vector<string>>();

    if (config.contains("parameters"))
      parameters = config["parameters"].get<string>();

    if (config.contains("sql"))
      sqlName = config["sql"].get<string>();

    // pass through the config
    driver->applyConfig(config);
  }

  ~ActiveAdapter() {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"
    delete driver;
#pragma GCC diagnostic pop
  }

private:
  ActiveDriver *driver;
  bool isOnline = true;
  int counter = 300;
  vector<string> waitingList;
  vector<string> proxies;
  int proxyCount = 0;
  std::mutex mutex;
  string parameters;
  string sqlName = "sqlite3";
  connection_pool *pool;

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

  void mainLoop() {
    while (!driversManager.isReady)
      this_thread::sleep_for(chrono::milliseconds(100));

    if (driversManager.get(this->id) == nullptr)
      return;

    // Connect to database
    try {
      int poolSize = thread::hardware_concurrency();
      pool = new connection_pool(poolSize);

      for (size_t i = 0; i != poolSize; ++i) {
        session &sql = pool->at(i);
        sql.open(sqlName, parameters);
      }

    } catch (...) {
      isOnline = false;
    }

    if (!isOnline)
      return log(fmt::format("ActiveDriver - {}", this->id),
                 "Failed to Connect to the Database");

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
          log(fmt::format("ActiveDriver - {}", this->id), "Getting Updates");
          manga = driver->getUpdates(proxy);
        } catch (...) {
          log(fmt::format("ActiveDriver - {}", this->id),
              "Failed to Get Updates");
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

        session sql(*pool);
        rowset<row> rs = sql.prepare << fmt::format(
                             "SELECT * FROM MANGA WHERE ID IN ({})", oss.str());

        for (auto it = rs.begin(); it != rs.end(); it++) {
          const row &row = *it;

          id = row.get<string>("ID");

          // check if updated and not in the waiting list
          if (!this->driver->isLatestEqual(row.get<string>("LATEST"),
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
          log(fmt::format("ActiveDriver - {}", this->id),
              fmt::format("Getting {}", id));
          DetailsManga *manga =
              (DetailsManga *)driver->getManga({id}, true, proxy).at(0);

          ostringstream categories;
          for (size_t i = 0; i < manga->categories.size(); ++i) {
            categories << categoryToString(manga->categories[i]);
            if (i < manga->categories.size() - 1)
              categories << "|";
          }

          session sql(*pool);
          transaction tr(sql);

          // update the manga info
          sql << "REPLACE INTO MANGA (ID, THUMBNAIL, TITLE, DESCRIPTION, "
                 "IS_ENDED, AUTHORS, CATEGORIES, LATEST, UPDATE_TIME, "
                 "EXTRA_DATA) VALUES (:id, :thumbnail, :title, :description, "
                 ":is_ended, :authors, :categories, :latest, :update_time, "
                 ":extras_data)",
              use(manga->id), use(manga->thumbnail), use(manga->title),
              use(manga->description), use((int)manga->isEnded),
              use(fmt::format("{}", fmt::join(manga->authors, "|"))),
              use(categories.str()), use(manga->latest),
              use(chrono::duration_cast<chrono::seconds>(
                      chrono::system_clock::now().time_since_epoch())
                      .count()),
              use(manga->chapters.extraData);

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
            sql << fmt::format("DELETE FROM CHAPTER WHERE MANGA_ID = :manga_id "
                               "AND ID NOT IN ({})",
                               oss.str()),
                use(manga->id);

            // insert only the new one
            rowset<row> rs =
                (sql.prepare
                     << "SELECT ID FROM CHAPTER WHERE MANGA_ID = :manga_id",
                 use(manga->id));
            for (auto it = rs.begin(); it != rs.end(); it++) {
              const row &row = *it;
              chaptersMap.erase(row.get<string>("ID"));
            }

            for (const auto &pair : chaptersMap) {
              Chapter chapter = chapters[pair.second];

              sql << "REPLACE INTO CHAPTER (MANGA_ID, ID, IDX, TITLE, "
                     "IS_EXTRA) VALUES (:manga_id, :id, :idx, :title, "
                     ":is_extra)",
                  use(manga->id), use(chapter.id),
                  use((int)(chapters.size() - pair.second - 1)),
                  use(chapter.title), use((int)isExtra);
            }
          };

          updateChapter(manga->chapters.serial, false);
          updateChapter(manga->chapters.extra, true);

          tr.commit();
        } catch (...) {
          log(fmt::format("ActiveDriver - {}", this->id),
              fmt::format("Failed to Get {}", id));
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