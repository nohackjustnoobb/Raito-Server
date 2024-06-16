#pragma once

#include "../../models/baseDriver.hpp"
#include "../../models/manga.hpp"
#include "../../utils/converter.hpp"

#include "node.hpp"

#include <cpr/cpr.h>

using namespace std;

class DM5 : public BaseDriver {
public:
  DM5();

  vector<Manga *> getManga(vector<string> ids, bool showDetails) override;

  vector<string> getChapter(string id, string extraData) override;

  vector<Manga *> getList(Category category, int page, Status status) override;

  vector<string> getSuggestion(string keyword) override;

  vector<Manga *> search(string keyword, int page) override;

  bool checkOnline() override;

private:
  map<Category, int> categoryId = {
      {All, 0},     {Passionate, 31}, {Love, 26},      {Campus, 1},
      {Yuri, 3},    {BL, 27},         {Adventure, 2},  {Harem, 8},
      {SciFi, 25},  {War, 12},        {Suspense, 17},  {Speculation, 33},
      {Funny, 37},  {Fantasy, 14},    {Magic, 15},     {Horror, 29},
      {Ghosts, 20}, {History, 4},     {FanFi, 30},     {Sports, 34},
      {Hentai, 36}, {Mecha, 40},      {Restricted, 61}};
  map<string, Category> categoryText = {
      {"rexue", Passionate},    {"aiqing", Love},
      {"xiaoyuan", Campus},     {"baihe", Yuri},
      {"caihong", BL},          {"maoxian", Adventure},
      {"hougong", Harem},       {"kehuan", SciFi},
      {"zhanzheng", War},       {"xuanyi", Suspense},
      {"zhentan", Speculation}, {"gaoxiao", Funny},
      {"qihuan", Fantasy},      {"mofa", Magic},
      {"kongbu", Horror},       {"dongfangshengui", Ghosts},
      {"lishi", History},       {"tongren", FanFi},
      {"jingji", Sports},       {"jiecao", Hentai},
      {"jizhan", Mecha},        {"list-tag61", Restricted}};
  string baseUrl = "https://www.dm5.com/";
  cpr::Header header = cpr::Header{
      {"Accept-Language", "en-US,en;q=0.5"},
      {"User-Agent",
       "Mozilla/5.0 (Macintosh; Intel Mac OS X 14_1) AppleWebKit/605.1.15 "
       "(KHTML, like Gecko) Version/17.2 Safari/605.1.15"}};
  Converter chineseConverter;

  Manga *extractManga(Node *node);

  Manga *extractDetails(Node *node, const string &id, const bool showDetails);

  vector<string> decodeChapters(const string &encoded, const int &len1,
                                const int &len2, const string &valuesString);
};