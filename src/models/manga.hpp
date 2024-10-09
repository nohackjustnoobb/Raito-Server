#pragma once

#include "../manager/driversManager.hpp"
#include "../utils/utils.hpp"
#include "baseDriver.hpp"
#include "common.hpp"

using json = nlohmann::json;

struct Chapter {
  // The title of the chapter.
  string title;
  // The id of the chapter. This should be unique for all chapters.
  string id;
};

struct Chapters {
  // The serial chapters of the manga.
  vector<Chapter> serial;
  // The extra chapters of the manga.
  vector<Chapter> extra;
  // The extra data required to get the chapter. Usually the id of the manga.
  string extraData;

  // This should not be called directly.
  json toJson() {
    json result;

    vector<map<string, string>> serial, extra;
    for (Chapter c : this->serial)
      serial.push_back({{"title", c.title}, {"id", c.id}});
    for (Chapter c : this->extra)
      extra.push_back({{"title", c.title}, {"id", c.id}});

    result["serial"] = serial;
    result["extra"] = extra;
    result["extraData"] = extraData;

    return result;
  }

  static Chapters fromJson(const json &data) {
    Chapters chapters;
    if (data.contains("extraData"))
      chapters.extraData = data["extraData"].get<string>();
    chapters.serial = {};
    chapters.extra = {};

    if (data.contains("serial"))
      for (const auto &chapter : data["serial"])
        chapters.serial.push_back(
            {chapter["title"].get<string>(), chapter["id"].get<string>()});

    if (data.contains("extra"))
      for (const auto &chapter : data["extra"])
        chapters.extra.push_back(
            {chapter["title"].get<string>(), chapter["id"].get<string>()});

    return chapters;
  }
};

class Manga {
public:
  // A pointer to the driver of the manga.
  BaseDriver *driver;
  // The id of the manga. Should only contain numbers and alphabetic characters.
  string id;
  // The name of the manga.
  string title;
  // The url of the manga's thumbnail.
  string thumbnail;
  // The latest chapter title of the manga.
  string latest;
  // Determine whether the manga is already ended.
  bool isEnded;

  Manga(BaseDriver *driver, const string &id, const string &title,
        const string &thumbnail, const string &latest, const bool &isEnded)
      : driver(driver), id(id), title(title), thumbnail(thumbnail),
        latest(latest), isEnded(isEnded) {}

  // Convert the manga object into json object
  virtual json toJson() {
    json result;

    result["driver"] = driver->id;
    result["id"] = id;
    result["title"] = title;
    result["thumbnail"] = thumbnail;
    result["latest"] = latest;
    result["isEnded"] = isEnded;

    return result;
  }

  // Apply proxy settings to the thumbnail
  void useProxy(const string &baseUrl) {
    thumbnail = driver->useProxy(thumbnail, "thumbnail", baseUrl);
  }
};

class DetailsManga : public Manga {
public:
  // The description of the manga.
  string description;
  // The genres of the manga.
  vector<Genre> genres;
  // The chapters of the manga.
  Chapters chapters;
  // The authors of the manga.
  vector<string> authors;
  // The update date of the manga, encoded in seconds since epoch.
  int *updateTime;

  DetailsManga(BaseDriver *driver, const string &id, const string &title,
               const string &thumbnail, const string &latest,
               const vector<string> &authors, const bool &isEnded,
               const string &description, const vector<Genre> &genres,
               const Chapters &chapters, int *updateTime = nullptr)
      : Manga(driver, id, title, thumbnail, latest, isEnded), authors(authors),
        description(description), genres(genres), chapters(chapters),
        updateTime(updateTime) {}

  ~DetailsManga() {
    if (updateTime != nullptr)
      delete updateTime;
  }

  // Convert the manga object into json object
  json toJson() override {
    json result;

    result["driver"] = driver->id;
    result["id"] = id;
    result["title"] = title;
    result["thumbnail"] = thumbnail;
    result["latest"] = latest;
    result["isEnded"] = isEnded;
    result["description"] = description;
    result["chapters"] = chapters.toJson();

    // FIXME temporary fix
    try {
      result["authors"] = authors;
      result.dump();
    } catch (...) {
      result["authors"] = json::array();
    }

    vector<string> genresString;
    for (Genre genre : genres)
      genresString.push_back(genreToString(genre));

    result["genres"] = genresString;

    if (updateTime != nullptr)
      result["updateTime"] = *updateTime;
    else
      result["updateTime"] = nullptr;

    return result;
  }

  static DetailsManga *fromJson(const json &data) {
    vector<Genre> genres;
    for (string genre : data["genres"])
      genres.push_back(stringToGenre(genre));

    int *updateTime;
    if (data.contains("updateTime") && !data["updateTime"].is_null())
      updateTime = new int(data["updateTime"].get<int>());

    return new DetailsManga(
        data.contains("driver")
            ? driversManager.get(data["driver"].get<string>())
            : nullptr,
        data["id"].get<string>(), data["title"].get<string>(),
        data["thumbnail"].get<string>(), data["latest"].get<string>(),
        data["authors"].get<vector<string>>(), data["isEnded"].get<bool>(),
        data["description"].get<string>(), genres,
        Chapters::fromJson(data["chapters"]), updateTime);
  }
};
