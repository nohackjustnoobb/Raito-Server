#include <iomanip>
#include <re2/re2.h>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

namespace MHG_utils {
template <typename T> static void releaseMemory(vector<T> vector) {
  for (T ptr : vector) {
    delete ptr;
  }
}

static string strip(const string &s) {
  string copy = s;
  RE2::GlobalReplace(&copy, RE2(R"(^\s+|\s+$)"), "");

  return copy;
}

static vector<string> split(string s, RE2 r) {
  vector<string> splits;

  // Use GlobalReplace to split the string
  RE2::GlobalReplace(&s, r, "\u001D");

  // Tokenize the modified string by \u001D
  std::istringstream tokenizer(s);
  std::string token;
  while (std::getline(tokenizer, token, '\u001D')) {
    splits.push_back(token);
  }

  return splits;
}

static string decompress(const string &encoded, const int &len1,
                         const int &len2, const string &valuesString) {

  vector<string> values = split(valuesString, RE2(R"(\|)"));
  values.push_back("");

  std::function<string(int)> genKey = [&](int index) -> string {
    std::function<string(int, int)> itr = [&](int value, int num) -> string {
      string d =
          "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
      return value <= 0 ? "" : itr(int(value / num), num) + d[value % num];
    };

    std::function<string(int, int)> tr = [&](int value, int num) -> string {
      string tmp = itr(value, num);
      return tmp == "" ? "0" : tmp;
    };

    int lastChar = index % len1;
    return (index < len1 ? "" : genKey(index / len1)) +
           (lastChar > 35 ? string(1, (char)(lastChar + 29))
                          : tr(lastChar, 36));
  };

  int i = len2 - 1;
  map<string, string> pairs;
  while (i + 1) {
    string key = genKey(i);
    pairs[key] = values.at(i) == "" ? key : values[i];
    i--;
  }

  string decoded = encoded;
  for (auto pair : pairs) {
    RE2::GlobalReplace(&decoded, RE2("\\b" + pair.first + "\\b"), pair.second);
  }

  return decoded;
}

} // namespace MHG_utils
