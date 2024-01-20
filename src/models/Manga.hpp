#pragma once

#include "BaseDriver.hpp"
#include "Common.hpp"

#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct Chapter {
  string title;
  string id;
};

struct Chapters {
  vector<Chapter> serial;
  vector<Chapter> extra;
  string extraData;

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
};

class Manga {
public:
  BaseDriver *driver;
  string id;
  string title;
  string thumbnail;
  string latest;
  bool isEnded;

  Manga(BaseDriver *driver, const string &id, const string &title,
        const string &thumbnail, const string &latest, const bool &isEnded)
      : driver(driver), id(id), title(title), thumbnail(thumbnail),
        latest(latest), isEnded(isEnded) {}

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

  void useProxy() { thumbnail = driver->useProxy(thumbnail, "thumbnail"); }
};

class DetailsManga : public Manga {
public:
  string description;
  vector<Category> categories;
  Chapters chapters;
  vector<string> authors;

  DetailsManga(BaseDriver *driver, const string &id, const string &title,
               const string &thumbnail, const string &latest,
               const vector<string> &authors, const bool &isEnded,
               const string &description, const vector<Category> &categories,
               const Chapters &chapters)
      : Manga(driver, id, title, thumbnail, latest, isEnded), authors(authors),
        description(description), categories(categories), chapters(chapters) {}

  json toJson() override {
    json result;

    result["driver"] = driver->id;
    result["id"] = id;
    result["title"] = title;
    result["thumbnail"] = thumbnail;
    result["latest"] = latest;
    result["isEnded"] = isEnded;
    result["description"] = description;
    result["authors"] = authors;
    result["chapters"] = chapters.toJson();

    vector<string> categoriesString;
    for (Category category : categories)
      categoriesString.push_back(categoryToString(category));

    result["categories"] = categoriesString;

    return result;
  }
};
