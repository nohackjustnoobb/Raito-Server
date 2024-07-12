
#include "selfContained.hpp"
#include "../../manager/driversManager.hpp"
#include "../../utils/utils.hpp"

#include <chrono>
#include <rapidfuzz/fuzz.hpp>

#define CHECK_ONLINE()                                                         \
  if (!isOnline)                                                               \
    throw "Database is offline";

SelfContained::SelfContained() {
  id = "SC";
  version = "1.0.0-beta.0";
  supportSuggestion = true;
  supportedCategories = {
      All,         Passionate, Love,    Campus, Yuri,   Otokonoko,
      BL,          Adventure,  Harem,   SciFi,  War,    Suspense,
      Speculation, Funny,      Fantasy, Magic,  Horror, Ghosts,
      History,     FanFi,      Sports,  Hentai, Mecha,  Restricted,
  };

  thread(&SelfContained::initializeDatabase, this).detach();
}

Manga *SelfContained::createManga(DetailsManga *manga) {
  CHECK_ONLINE()

  // Update the placeholder and remove unnecessary data
  manga->id = generateId();
  manga->updateTime =
      new int(chrono::duration_cast<chrono::seconds>(
                  chrono::system_clock::now().time_since_epoch())
                  .count());
  // manga->thumbnail should be a placeholder which it should be uploaded
  // through the image uploading endpoint.
  manga->thumbnail = "";
  // manga->chapters should also be a placeholder which it should updated
  // through chaper endpoint.
  manga->chapters = {};
  manga->chapters.extraData = manga->id;
  // manga->latest should be empty
  manga->latest = "";

  // Insert the manga
  session sql(*pool);

  ostringstream categories;
  for (size_t i = 0; i < manga->categories.size(); ++i) {
    categories << categoryToString(manga->categories[i]);
    if (i < manga->categories.size() - 1)
      categories << "|";
  }

  sql << "INSERT INTO MANGA (ID, THUMBNAIL, TITLE, DESCRIPTION, IS_ENDED, "
         "AUTHORS, CATEGORIES, LATEST, UPDATE_TIME, EXTRA_DATA) VALUES (:id, "
         ":thumbnail, :title, :description, :is_ended, :authors, "
         ":categories, :latest, :update_time, :extras_data)",
      use(manga->id), use(manga->thumbnail), use(manga->title),
      use(manga->description), use((int)manga->isEnded),
      use(fmt::format("{}", fmt::join(manga->authors, "|"))),
      use(categories.str()), use(manga->latest), use(*manga->updateTime),
      use(manga->chapters.extraData);

  row r;
  sql << "SELECT * FROM MANGA WHERE ID = :id", use(manga->id), into(r);

  // release the memory
  delete manga;

  return toManga(r, true);
}

Manga *SelfContained::editManga(DetailsManga *manga) {
  CHECK_ONLINE()

  // Insert the manga
  session sql(*pool);

  ostringstream categories;
  for (size_t i = 0; i < manga->categories.size(); ++i) {
    categories << categoryToString(manga->categories[i]);
    if (i < manga->categories.size() - 1)
      categories << "|";
  }

  sql << "UPDATE MANGA SET TITLE = :title, DESCRIPTION = :description, "
         "IS_ENDED = :is_ended, AUTHORS = :authors, CATEGORIES = "
         ":categories WHERE ID = :id",
      use(manga->title), use(manga->description), use((int)manga->isEnded),
      use(fmt::format("{}", fmt::join(manga->authors, "|"))),
      use(categories.str()), use(manga->id);

  row r;
  sql << "SELECT * FROM MANGA WHERE ID = :id", use(manga->id), into(r);

  // release the memory
  delete manga;

  return toManga(r, true);
}

void SelfContained::deleteManga(string id) {
  CHECK_ONLINE()

  session sql(*pool);

  // Delete chapters
  rowset<row> rs =
      (sql.prepare << "SELECT ID FROM CHAPTER WHERE MANGA_ID = :id", use(id));

  vector<string> ids;
  for (auto it = rs.begin(); it != rs.end(); it++) {
    const row &row = *it;
    ids.push_back(row.get<string>("ID"));
  }

  for (const auto &chapterId : ids)
    deleteChapter(chapterId, id);

  // Delete manga
  sql << "DELETE FROM MANGA WHERE ID = :id", use(id);
}

Chapters SelfContained::createChapter(string extraData, string title,
                                      bool isExtra) {
  CHECK_ONLINE()

  // Create a new chapter
  int index = generateIndex(extraData);
  string id = generateChapterId();

  session sql(*pool);
  sql << "INSERT INTO CHAPTER (MANGA_ID, ID, IDX, TITLE, IS_EXTRA) VALUES "
         "(:manga_id, :id, :idx, :title, :is_extra)",
      use(extraData), use(id), use(index), use(title), use((int)isExtra);

  sql << "UPDATE MANGA SET UPDATE_TIME = :update_time, LATEST = :latest "
         "WHERE ID = :id",
      use(chrono::duration_cast<chrono::seconds>(
              chrono::system_clock::now().time_since_epoch())
              .count()),
      use(title), use(extraData);

  return getChapters(extraData);
}

Chapters SelfContained::editChapters(Chapters chapters) {
  CHECK_ONLINE()

  session sql(*pool);

  // get the previous chapters
  rowset<string> rs = (sql.prepare << "SELECT ID FROM CHAPTER WHERE MANGA_ID = "
                                      ":id ORDER BY -IDX",
                       use(chapters.extraData));
  vector<string> ids;
  copy(rs.begin(), rs.end(), back_inserter(ids));

  transaction tr(sql);
  try {
    // update the chapters
    auto updateChapters = [&](vector<Chapter> chapterVector, bool isExtra) {
      for (int i = 0; i < chapterVector.size(); i++) {
        sql << "UPDATE CHAPTER SET IDX = :idx, TITLE = :title, IS_EXTRA = "
               ":is_extra WHERE MANGA_ID = :manga_id AND ID = :id",
            use(chapterVector.size() - i - 1), use(chapterVector[i].title),
            use((int)isExtra), use(chapters.extraData),
            use(chapterVector[i].id);

        auto it = find(ids.begin(), ids.end(), chapterVector[i].id);
        if (it == ids.end())
          throw "Chapter not found";

        ids.erase(it);
      }
    };

    updateChapters(chapters.serial, false);
    updateChapters(chapters.extra, true);

    // check if all the chapters are updated
    if (ids.empty())
      tr.commit();
    else
      throw "Missing chapter";
  } catch (...) {
    tr.rollback();
  }

  return getChapters(chapters.extraData);
}

void SelfContained::deleteChapter(string id, string extraData) {
  CHECK_ONLINE()

  session sql(*pool);

  // delete the image first
  string urls;
  indicator ind;
  sql << "SELECT URLS FROM CHAPTER WHERE MANGA_ID = :extra_data AND ID = :id",
      use(extraData), use(id), into(urls, ind);
  if (ind != i_null)
    for (const auto &hash : split(urls, R"(\|)"))
      imagesManager.deleteImage(this->id, "manga", hash);

  sql << "DELETE FROM CHAPTER WHERE MANGA_ID = :extra_data AND ID = :id",
      use(extraData), use(id);
}

vector<string> SelfContained::uploadThumbnail(string id, const string &image) {
  CHECK_ONLINE()

  string thumbnail = imagesManager.saveImage(this->id, "thumbnail", image);

  session sql(*pool);

  // delete the old thumbnail
  string oldThumbnail;
  sql << "SELECT THUMBNAIL FROM MANGA WHERE ID = :id", use(id),
      into(oldThumbnail);
  if (!oldThumbnail.empty())
    imagesManager.deleteImage(this->id, "thumbnail", oldThumbnail);

  // insert the new thumbnail
  sql << "UPDATE MANGA SET THUMBNAIL = :thumbnail WHERE ID = :id",
      use(thumbnail), use(id);

  return imagesManager.getImage(this->id, "thumbnail", thumbnail, false);
}

vector<string> SelfContained::uploadMangaImage(string id, string extraData,
                                               const string &image) {
  CHECK_ONLINE()

  string manga = imagesManager.saveImage(this->id, "manga", image);

  session sql(*pool);

  // get the previous urls
  string urls;
  indicator ind;
  sql << "SELECT URLS FROM CHAPTER WHERE MANGA_ID = :extra_data AND ID = :id",
      use(extraData), use(id), into(urls, ind);

  if (ind != i_null)
    urls += "|";
  urls += manga;

  statement st = (sql.prepare << "UPDATE CHAPTER SET URLS = :urls WHERE "
                                 "MANGA_ID = :extra_data AND "
                                 "ID = :id",
                  use(urls), use(extraData), use(id));
  st.execute(true);

  // if the database is not updated, that means something went wrong and the
  // image should be deleted.
  if (!st.get_affected_rows())
    imagesManager.deleteImage(this->id, "manga", manga);

  return imagesManager.getImage(this->id, "manga", manga, false);
}

vector<string> SelfContained::uploadMangaImages(string id, string extraData,
                                                vector<string> images) {
  CHECK_ONLINE()

  vector<string> newUrls;
  for (const auto &image : images)
    newUrls.push_back(imagesManager.saveImage(this->id, "manga", image));

  session sql(*pool);

  // get the previous urls
  string urls;
  indicator ind;
  sql << "SELECT URLS FROM CHAPTER WHERE MANGA_ID = :extra_data AND ID = :id",
      use(extraData), use(id), into(urls, ind);

  if (ind != i_null && !urls.empty())
    urls += "|";
  urls += fmt::format("{}", fmt::join(newUrls, "|"));

  statement st = (sql.prepare << "UPDATE CHAPTER SET URLS = :urls WHERE "
                                 "MANGA_ID = :extra_data AND "
                                 "ID = :id",
                  use(urls), use(extraData), use(id));
  st.execute(true);

  // if the database is not updated, that means something went wrong and the
  // image should be deleted.
  if (!st.get_affected_rows())
    for (const auto &url : newUrls)
      imagesManager.deleteImage(this->id, "manga", url);

  return getChapter(id, extraData);
}

vector<string> SelfContained::arrangeMangaImage(string id, string extraData,
                                                vector<string> newUrls) {
  CHECK_ONLINE()

  session sql(*pool);

  vector<string> newHashes;
  string hash;

  for (auto const &url : newUrls) {
    if (RE2::PartialMatch(url, R"(\/(.{32})\.webp)", &hash))
      newHashes.push_back(hash);
  }

  string urls;
  sql << "SELECT URLS FROM CHAPTER WHERE MANGA_ID = :extra_data AND ID = :id",
      use(extraData), use(id), into(urls);
  vector<string> oldHashes = split(urls, R"(\|)");

  try {
    for (const auto &hash : newHashes) {
      auto it = find(oldHashes.begin(), oldHashes.end(), hash);
      if (it == oldHashes.end())
        throw "Hash not found";

      oldHashes.erase(it);
    }

    if (!oldHashes.empty())
      throw "Missing hash";

    sql << fmt::format("UPDATE CHAPTER SET URLS = \"{}\" WHERE MANGA_ID = "
                       ":extra_data AND ID = :id",
                       fmt::join(newHashes, "|")),
        use(extraData), use(id);
  } catch (...) {
  }

  return getChapter(id, extraData);
}

void SelfContained::deleteMangaImage(string id, string extraData, string url) {
  CHECK_ONLINE()

  string hash = url;
  RE2::PartialMatch(url, R"(\/(.{32})\.webp)", &hash);

  imagesManager.deleteImage(this->id, "manga", hash);

  session sql(*pool);

  // get the urls
  string urls;
  sql << "SELECT URLS FROM CHAPTER WHERE MANGA_ID = :extra_data AND ID = :id",
      use(extraData), use(id), into(urls);

  // remove the hash from the urls
  RE2::GlobalReplace(&urls, R"(\|?)" + hash, "");
  if (!urls.empty() && urls.rfind("|", 0) == 0)
    urls.erase(urls.begin());

  sql << "UPDATE CHAPTER SET URLS = :urls WHERE MANGA_ID = :extra_data AND ID "
         "= :id",
      use(urls), use(extraData), use(id);
}

string SelfContained::useProxy(const string &dest, const string &genre,
                               const string &baseUrl) {
  return imagesManager.getLocalPath(this->id, genre, dest, baseUrl);
}

void SelfContained::applyConfig(json config) {
  LocalDriver::applyConfig(config);

  if (config.contains("id"))
    id = config["id"].get<string>();
}

Chapters SelfContained::getChapters(string extraData) {
  // Query all the chapters
  Chapters chapters;
  chapters.extraData = extraData;
  chapters.extra = {};
  chapters.serial = {};

  session sql(*pool);
  rowset<row> rs = (sql.prepare << "SELECT * FROM CHAPTER WHERE MANGA_ID = "
                                   ":id ORDER BY -IDX",
                    use(extraData));

  for (auto it = rs.begin(); it != rs.end(); it++) {
    const row &row = *it;
    Chapter chapter = {row.get<string>("TITLE"), row.get<string>("ID")};

    if (row.get<int>("IS_EXTRA") == 1)
      chapters.extra.push_back(chapter);
    else
      chapters.serial.push_back(chapter);
  }

  return chapters;
}

string SelfContained::generateId() {
  session sql(*pool);
  string lastId;
  indicator ind;

  sql << "SELECT ID FROM MANGA ORDER BY -LENGTH(ID),-ID LIMIT 1",
      into(lastId, ind);

  if (ind == i_null)
    return "1";

  return to_string(stoi(lastId) + 1);
}

string SelfContained::generateChapterId() {
  session sql(*pool);
  string lastId;
  indicator ind;

  sql << "SELECT ID FROM CHAPTER ORDER BY -LENGTH(ID),-ID LIMIT 1",
      into(lastId, ind);

  if (ind == i_null)
    return "1";

  return to_string(stoi(lastId) + 1);
}

int SelfContained::generateIndex(string id) {
  session sql(*pool);
  int lastIndex;
  indicator ind;

  sql << "SELECT IDX FROM CHAPTER WHERE MANGA_ID = :id ORDER BY -IDX LIMIT 1",
      use(id), into(lastIndex, ind);

  if (ind == i_null)
    return 0;

  return lastIndex + 1;
}
