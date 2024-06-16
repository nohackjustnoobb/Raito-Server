#pragma once

#include <string>
using namespace std;

// Forward declarations.

class Manga;

class DetailsManga;

// Only used to check if the manga is up to date.
struct PreviewManga {
  string id;
  string latest;
};

enum Status {
  Any,
  OnGoing,
  Ended,
};

enum Category {
  All,
  Passionate,
  Love,
  Campus,
  Yuri,
  Otokonoko,
  BL,
  Adventure,
  Harem,
  SciFi,
  War,
  Suspense,
  Speculation,
  Funny,
  Fantasy,
  Magic,
  Horror,
  Ghosts,
  History,
  FanFi,
  Sports,
  Hentai,
  Mecha,
  Restricted,
};

static string categoryString[] = {
    "All",         "Passionate", "Love",    "Campus", "Yuri",   "Otokonoko",
    "BL",          "Adventure",  "Harem",   "SciFi",  "War",    "Suspense",
    "Speculation", "Funny",      "Fantasy", "Magic",  "Horror", "Ghosts",
    "History",     "FanFi",      "Sports",  "Hentai", "Mecha",  "Restricted"};

static string categoryToString(Category category) {
  return categoryString[category];
}

static Category stringToCategory(string category) {
  for (int i = 0; i < sizeof(categoryString) / sizeof(categoryString[0]); i++) {
    if (category == categoryString[i])
      return (Category)i;
  }

  return All;
}
