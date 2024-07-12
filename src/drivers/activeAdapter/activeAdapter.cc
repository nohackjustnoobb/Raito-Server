#include "activeAdapter.hpp"
#include "../../manager/driversManager.hpp"
#include "../../utils/utils.hpp"

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

ActiveAdapter::ActiveAdapter(ActiveDriver *driver) : driver(driver) {
  id = driver->id;
  proxyHeaders = driver->proxyHeaders;
  supportedCategories = driver->supportedCategories;
  version = driver->version;
  supportSuggestion = true;

  // start a thread
  thread(&ActiveAdapter::mainLoop, this).detach();
}

ActiveAdapter::~ActiveAdapter() {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"
  delete driver;
#pragma GCC diagnostic pop
}

vector<string> ActiveAdapter::getChapter(string id, string extraData) {
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

    sql << fmt::format("UPDATE CHAPTER SET URLS = '{}' WHERE ID = :id AND "
                       "MANGA_ID = :manga_id",
                       fmt::join(result, "|")),
        use(id), use(extraData);

    return result;
  } else {
    return split(urls, R"(\|)");
  }
}

void ActiveAdapter::applyConfig(json config) {
  if (config.contains("proxies"))
    proxies = config["proxies"].get<vector<string>>();

  LocalDriver::applyConfig(config);

  // pass through the config
  driver->applyConfig(config);
}

void ActiveAdapter::mainLoop() {
  while (!driversManager.isReady)
    this_thread::sleep_for(chrono::milliseconds(100));

  if (driversManager.get(this->id) == nullptr)
    return;

  initializeDatabase();

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
