#include <cpr/cpr.h>
#include <iomanip>
#include <re2/re2.h>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

namespace MHR_utils {
static cpr::Parameters mapToParameters(const map<string, string> &query) {
  cpr::Parameters parameters;

  for (const auto &pair : query) {
    parameters.Add({pair.first, pair.second});
  }

  return parameters;
}

static string urlEncode(const string &text) {
  ostringstream encoded;
  encoded.fill('0');
  encoded << uppercase << hex;

  for (char c : text) {
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded << c;
    } else if (c == ' ') {
      encoded << '+';
    } else {
      encoded << '%' << setw(2) << int(static_cast<unsigned char>(c));
    }
  }

  string encodedText = encoded.str();

  RE2::GlobalReplace(&encodedText, RE2("\\+"), "%20");
  RE2::GlobalReplace(&encodedText, RE2("\\%7E"), "~");
  RE2::GlobalReplace(&encodedText, RE2("\\*"), "%2A");

  return encodedText;
}

static vector<string> split(string s, RE2 r) {
  vector<string> splits;

  // Use GlobalReplace to split the string
  RE2::GlobalReplace(&s, r, "\u001D");

  // Tokenize the modified string by \u001D
  istringstream tokenizer(s);
  string token;
  while (getline(tokenizer, token, '\u001D')) {
    splits.push_back(token);
  }

  return splits;
}

} // namespace MHR_utils