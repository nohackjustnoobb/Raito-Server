#pragma once

#include "../../models/ActiveDriver.hpp"
#include "node.hpp"

#include <codecvt>

using namespace std;

class MHG : public ActiveDriver {
public:
  MHG();

  vector<Manga *> getManga(vector<string> ids, bool showDetails,
                           string proxy) override;

  vector<string> getChapter(string id, string extraData, string proxy) override;

  vector<Manga *> getList(Category category, int page, Status status) override;

  vector<Manga *> search(string keyword, int page) override;

  vector<PreviewManga> getUpdates(string proxy = "") override;

  bool checkOnline() override;

private:
// disable warning for deprecated functions
// which don't have standardized replacements
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  wstring_convert<codecvt_utf8<char16_t>, char16_t> convert;
#pragma GCC diagnostic pop

  string baseUrl = "https://tw.manhuagui.com/";
  map<Category, string> categoryText = {{All, ""},
                                        {Passionate, "rexue"},
                                        {Love, "aiqing"},
                                        {Campus, "xiaoyuan"},
                                        {Yuri, "baihe"},
                                        {BL, "danmei"},
                                        {Adventure, "maoxian"},
                                        {Harem, "hougong"},
                                        {SciFi, "kehuan"},
                                        {War, "zhanzheng"},
                                        {Suspense, "xuanyi"},
                                        {Speculation, "tuili"},
                                        {Funny, "gaoxiao"},
                                        {Fantasy, "mohuan"},
                                        {Magic, "mofa"},
                                        {Horror, "kongbu"},
                                        {Ghosts, "shengui"},
                                        {History, "lishi"},
                                        {Sports, "jingji"},
                                        {Mecha, "jizhan"},
                                        {Otokonoko, "weiniang"}};

  Manga *extractDetails(Node *node, const string &id, const bool showDetails);

  Manga *extractManga(Node *node);

  vector<string> decodeChapters(const string &encoded, const int &len1,
                                const int &len2, const string &valuesString);
};