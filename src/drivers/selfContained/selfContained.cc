
#include "selfContained.hpp"
#include "../../manager/driversManager.hpp"
#include "../../utils/utils.hpp"

#include <iostream>
#include <libzippp/libzippp.h>

#define CHECK_ONLINE()                                                         \
  if (!isOnline)                                                               \
    throw "Database is offline";

using namespace libzippp;

SelfContained::SelfContained() {
  id = "SC";
  version = "1.0.0";
  supportSuggestion = true;
  supportedGenres = {
      All,         HotBlooded, Romance, Campus, Yuri,   Otokonoko,
      BL,          Adventure,  Harem,   SciFi,  War,    Suspense,
      Speculation, Funny,      Fantasy, Magic,  Horror, Ghosts,
      Historical,  FanFi,      Sports,  Hentai, Mecha,  Restricted,
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

  string encodedAuthors = fmt::format("{}", fmt::join(manga->authors, "|"));

  vector<string> genres;
  for (const auto &genre : manga->genres)
    genres.push_back(genreToString(genre));
  string encodedGenres = fmt::format("{}", fmt::join(genres, "|"));

  sql << "INSERT INTO MANGA (ID, THUMBNAIL, TITLE, DESCRIPTION, IS_ENDED, "
         "AUTHORS, GENRES, LATEST, UPDATE_TIME, EXTRA_DATA) VALUES (:id, "
         ":thumbnail, :title, :description, :is_ended, :authors, "
         ":genres, :latest, :update_time, :extras_data)",
      use(manga->id), use(manga->thumbnail), use(manga->title),
      use(manga->description), use((int)manga->isEnded), use(encodedAuthors),
      use(encodedGenres), use(manga->latest), use(*manga->updateTime),
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

  string encodedAuthors = fmt::format("{}", fmt::join(manga->authors, "|"));

  vector<string> genres;
  for (const auto &genre : manga->genres)
    genres.push_back(genreToString(genre));
  string encodedGenres = fmt::format("{}", fmt::join(genres, "|"));

  sql << "UPDATE MANGA SET TITLE = :title, DESCRIPTION = :description, "
         "IS_ENDED = :is_ended, AUTHORS = :authors, GENRES = "
         ":genres WHERE ID = :id",
      use(manga->title), use(manga->description), use((int)manga->isEnded),
      use(encodedAuthors), use(encodedGenres), use(manga->id);

  Manga *newManga = getManga({manga->id}, true).at(0);

  // release the memory
  delete manga;

  return newManga;
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

  // delete thumbnail
  Manga *manga = getManga({id}, false).at(0);
  if (manga->thumbnail != "")
    imagesManager.deleteImage(this->id, "thumbnail", manga->thumbnail);
  delete manga;

  // Delete manga
  sql << "DELETE FROM MANGA WHERE ID = :id", use(id);
}

Chapters SelfContained::createChapter(string extraData, string title,
                                      bool isExtra) {
  createChapterReturnId(extraData, title, isExtra);

  return getChapters(extraData);
}

string SelfContained::createChapterReturnId(string extraData, string title,
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

  return id;
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

vector<string> SelfContained::downloadManga(string id, bool asCBZ) {
  // Get the manga info
  vector<Manga *> mangas = getManga({id}, true);

  if (mangas.size() != 1)
    throw "Manga Not Found";

  Manga *manga = mangas[0];
  string title(manga->title);
  RE2::GlobalReplace(&title, R"(\/)", " ");

  // setup zip file
  bool failed = false;
  void *buffer = calloc(4096, sizeof(char));
  ZipArchive *zip =
      ZipArchive::fromWritableBuffer(&buffer, 4096, ZipArchive::New);
  string data;

  try {
    // add thumbnail
    vector<string> pre;
    string info;
    if (!asCBZ) {
      pre = imagesManager.getImage(this->id, "thumbnail", manga->thumbnail,
                                   false);
      zip->addData(fmt::format("{}/thumbnail.{}", title, pre[0]),
                   pre[1].c_str(), pre[1].size());

      info = manga->toJson().dump();
      zip->addData(fmt::format("{}/info.json", title), info.c_str(),
                   info.size());
    }

    session sql(*pool);
    indicator ind;
    rowset<row> rs =
        (sql.prepare << "SELECT * FROM CHAPTER WHERE MANGA_ID = :manga_id",
         use(id));
    vector<vector<string>> imgs;
    vector<string> cbz;

    for (auto it = rs.begin(); it != rs.end(); it++) {
      const row &row = *it;

      bool isExtra = row.get<int>("IS_EXTRA") == 1;
      string ctitle = row.get<string>("TITLE");
      RE2::GlobalReplace(&ctitle, R"(\/)", " ");
      string path =
          asCBZ ? fmt::format("{}/{}/", title, isExtra ? "Extra" : "Serial")
                : fmt::format("{}/{}/{}/", title, isExtra ? "Extra" : "Serial",
                              ctitle);
      zip->addEntry(path);

      vector<string> urls;

      // get the urls
      ind = row.get_indicator("URLS");
      if (ind != i_null)
        urls = split(row.get<string>("URLS"), R"(\|)");

      // create secondary zip for CBZ
      void *secBuffer = calloc(4096, sizeof(char));
      ZipArchive *secZip =
          ZipArchive::fromWritableBuffer(&secBuffer, 4096, ZipArchive::New);

      for (int i = 0; i < urls.size(); i++) {
        imgs.push_back(
            imagesManager.getImage(this->id, "manga", urls[i], false));

        if (asCBZ)
          secZip->addData(fmt::format("{}.{}", i, imgs.back()[0]),
                          imgs.back()[1].c_str(), imgs.back()[1].size());
        else
          zip->addData(fmt::format("{}{}.{}", path, i, imgs.back()[0]),
                       imgs.back()[1].c_str(), imgs.back()[1].size());
      }

      secZip->close();

      if (asCBZ) {
        imgs.clear();
        cbz.push_back(string((char *)secBuffer, secZip->getBufferLength()));
        zip->addData(fmt::format("{}{}.cbz", path, ctitle), cbz.back().c_str(),
                     cbz.back().size());
      }

      ZipArchive::free(secZip);
      free(secBuffer);
    }

    zip->close();
    int bufferContentLength = zip->getBufferLength();

    data = string((char *)buffer, bufferContentLength);
  } catch (...) {
    failed = true;
  }

  ZipArchive::free(zip);
  free(buffer);
  delete manga;

  if (failed)
    throw "Failed to Create Zip";

  return {title + (asCBZ ? ".zip" : ".raito.zip"), data};
}

DetailsManga *SelfContained::uploadManga(string file) {
  bool failed = false;
  string id;

  ZipArchive *zf = ZipArchive::fromBuffer(file.c_str(), file.size());

  try {
    if (zf == nullptr)
      throw "Invalid zip archive";

    ZipEntry thumb;
    json info;
    map<string, vector<ZipEntry>> serial = {};
    map<string, vector<ZipEntry>> extra = {};

    vector<ZipEntry> entries = zf->getEntries();
    for (const auto &entry : entries) {
      if (RE2::FullMatch(entry.getName(), R"(.*\/thumbnail\..*)"))
        thumb = entry;

      if (RE2::FullMatch(entry.getName(), R"(.*\/info.json)"))
        info = json::parse(entry.readAsText());

      string index;
      string title;
      if (RE2::FullMatch(entry.getName(), R"(.*\/Serial\/(.*)\/(.*)\..*)",
                         &title, &index)) {
        int idx = stoi(index);
        serial[title].insert(serial[title].begin() + idx, entry);
      } else if (RE2::FullMatch(entry.getName(), R"(.*\/Extra\/(.*)\/(.*)\..*)",
                                &title, &index)) {
        int idx = stoi(index);
        extra[title].insert(extra[title].begin() + idx, entry);
      }
    }

    // create the manga
    Manga *result = createManga(DetailsManga::fromJson(info));
    id = result->id;

    // upload the thumbnail
    uploadThumbnail(id, thumb.readAsText());

    auto uploadChapters = [&](const vector<json> &chapters,
                              const map<string, vector<ZipEntry>> &chapterMap) {
      for (auto rit = chapters.rbegin(); rit != chapters.rend(); ++rit) {
        json chapter = *rit;
        string title = chapter["title"].get<string>();
        string cid = createChapterReturnId(id, title, false);

        if (chapterMap.contains(title)) {
          vector<string> imgs;
          imgs.reserve(chapterMap.at(title).size());

          transform(chapterMap.at(title).begin(), chapterMap.at(title).end(),
                    back_inserter(imgs),
                    [](ZipEntry e) { return e.readAsText(); });

          uploadMangaImages(cid, id, imgs);
        }
      }
    };

    // create the chapter and upload the image if it exists
    vector<json> serialInfo = info["chapters"]["serial"].get<vector<json>>();
    vector<json> extraInfo = info["chapters"]["extra"].get<vector<json>>();

    uploadChapters(serialInfo, serial);
    uploadChapters(extraInfo, extra);
  } catch (...) {
    failed = true;
  }

  ZipArchive::free(zf);

  if (failed)
    throw "Failed to Upload a Manga";

  return (DetailsManga *)getManga({id}, true).at(0);
}

Chapters SelfContained::uploadChapter(string extraData, string title,
                                      bool isExtra, string file) {
  bool failed = false;
  ZipArchive *zf = ZipArchive::fromBuffer(file.c_str(), file.size());

  try {
    if (zf == nullptr)
      throw "Invalid zip archive";

    string id = createChapterReturnId(extraData, title, isExtra);

    vector<string> imgs;
    vector<ZipEntry> entries = zf->getEntries();
    for (const auto &entry : entries) {
      imgs.push_back(entry.readAsText());
    }

    uploadMangaImages(id, extraData, imgs);
  } catch (...) {
    failed = true;
  }

  ZipArchive::free(zf);

  if (failed)
    throw "Failed to Upload a Chapter";

  return getChapters(extraData);
}