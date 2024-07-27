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

enum Genre {
  All,
  HotBlooded,
  Romance,
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
  Historical,
  FanFi,
  Sports,
  Hentai,
  Mecha,
  Restricted,
};

static string genreString[] = {
    "All",         "HotBlooded", "Romance", "Campus", "Yuri",   "Otokonoko",
    "BL",          "Adventure",  "Harem",   "SciFi",  "War",    "Suspense",
    "Speculation", "Funny",      "Fantasy", "Magic",  "Horror", "Ghosts",
    "Historical",  "FanFi",      "Sports",  "Hentai", "Mecha",  "Restricted"};

static string genreToString(Genre category) { return genreString[category]; }

static Genre stringToGenre(string category) {
  for (int i = 0; i < sizeof(genreString) / sizeof(genreString[0]); i++) {
    if (category == genreString[i])
      return (Genre)i;
  }

  return All;
}
