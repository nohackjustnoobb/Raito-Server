#pragma once

#include "../../models/BaseDriver.hpp"
#include "converter.hpp"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace std;

class MHR : public BaseDriver {
public:
  MHR();

  vector<Manga *> getManga(vector<string> ids, bool showDetails) override;

  vector<string> getChapter(string id, string extraData) override;

  vector<Manga *> getList(Category category, int page, Status status) override;

  vector<string> getSuggestion(string keyword) override;

  vector<Manga *> search(string keyword, int page) override;

  bool checkOnline() override;

private:
  // disable warning for deprecated functions
// which don't have standardized replacements
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  wstring_convert<codecvt_utf8_utf16<wchar_t>> wstringConverter;
#pragma GCC diagnostic pop

  string hashKey = "4e0a48e1c0b54041bce9c8f0e036124d";
  string baseUrl = "https://hkmangaapi.manhuaren.com/";
  Converter chineseConverter;
  map<string, string> baseQuery = {
      {"gak", "android_manhuaren2"},
      {"gaui", "462099841"},
      {"gft", "json"},
      {"gui", "462099841"},
  };
  cpr::Header header = cpr::Header{
      {"Authorization",
       R"(YINGQISTS2 eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc19mcm9tX3JndCI6ZmFsc2UsInVzZXJfbGVnYWN5X2lkIjo0NjIwOTk4NDEsImRldmljZV9pZCI6Ii0zNCw2OSw2MSw4MSw2LDExNCw2MSwtMzUsLTEsNDgsNiwzNSwtMTA3LC0xMjIsLTExLC04NywxMjcsNjQsLTM4LC03LDUwLDEzLC05NCwtMTcsLTI3LDkyLC0xNSwtMTIwLC0zNyw3NCwtNzksNzgiLCJ1dWlkIjoiOTlmYTYzYjQtNjFmNy00ODUyLThiNDMtMjJlNGY3YzY2MzhkIiwiY3JlYXRldGltZV91dGMiOiIyMDIzLTA3LTAzIDAyOjA1OjMwIiwibmJmIjoxNjg4MzkzMTMwLCJleHAiOjE2ODgzOTY3MzAsImlhdCI6MTY4ODM5MzEzMH0.IJAkDs7l3rEvURHiy06Y2STyuiIu-CYUk5E8en4LU0_mrJ83hKZR1nVqKiAY9ry_6ZmFzVfg-ap_TXTF6GTqihyM-nmEpD2NVWeWZ5VHWVgJif4ezB4YTs0YEpnVzYCk_x4p0wU2GYbqf1BFrNO7PQPMMPDGfaCTUqI_Pe2B0ikXMaN6CDkMho26KVT3DK-xytc6lO92RHvg65Hp3xC1qaonQXdws13wM6WckUmrswItroy9z38hK3w0rQgXOK2mu3o_4zOKLGfq5JpqOCNQCLJgQ0_jFXhMtaz6E_fMZx54fZHfF1YrA-tfs7KFgiYxMb8PnNILoniFrQhvET3y-Q)"},
      {"X-Yq-Yqci",
       R"({"av":"1.3.8","cy":"HK","lut":"1662458886867","nettype":1,"os":2,"di":"733A83F2FD3B554C3C4E4D46A307D560A52861C7","fcl":"appstore","fult":"1662458886867","cl":"appstore","pi":"","token":"","fut":"1662458886867","le":"en-HK","ps":"1","ov":"16.4","at":2,"rn":"1668x2388","ln":"","pt":"com.CaricatureManGroup.CaricatureManGroup","dm":"iPad8,6"})"},
      {"User-Agent",
       R"(Mozilla/5.0 (Linux; Android 12; sdk_gphone64_arm64 Build/SE1A.220630.001; wv) AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Chrome/91.0.4472.114 Mobile Safari/537.36)"}};

  map<Category, int> categoryId = {
      {All, 0},     {Passionate, 31}, {Love, 26},       {Campus, 1},
      {Yuri, 3},    {BL, 27},         {Adventure, 2},   {Harem, 8},
      {SciFi, 25},  {War, 12},        {Suspense, 17},   {Speculation, 33},
      {Funny, 37},  {Fantasy, 14},    {Magic, 15},      {Horror, 29},
      {Ghosts, 20}, {History, 4},     {FanFi, 30},      {Sports, 34},
      {Hentai, 36}, {Mecha, 40},      {Restricted, 61}, {Otokonoko, 5}};
  map<string, Category> categoryText = {
      {"热血", Passionate}, {"恋爱", Love},         {"爱情", Love},
      {"校园", Campus},     {"百合", Yuri},         {"彩虹", BL},
      {"冒险", Adventure},  {"后宫", Harem},        {"科幻", SciFi},
      {"战争", War},        {"悬疑", Suspense},     {"推理", Speculation},
      {"搞笑", Funny},      {"奇幻", Fantasy},      {"魔法", Magic},
      {"恐怖", Horror},     {"神鬼", Ghosts},       {"历史", History},
      {"同人", FanFi},      {"运动", Sports},       {"绅士", Hentai},
      {"机甲", Mecha},      {"限制级", Restricted}, {"伪娘", Otokonoko}};

  string hash(vector<string> &list);

  string hash(map<string, string> query);

  string hash(map<string, string> query, string body);

  Manga *convert(const json &data);

  Manga *convertDetails(const json &data);

  string extractThumbnail(const string &url);

  vector<string> extractAuthors(const string authorsString);
};