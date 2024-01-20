#include <cpr/cpr.h>
#include <iomanip>
#include <openssl/evp.h>
#include <openssl/rand.h>
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

static string md5(const string &text) {

  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int hashLength;

  EVP_MD_CTX *md5ctx;
  const EVP_MD *md5 = EVP_md5();

  md5ctx = EVP_MD_CTX_new();
  EVP_DigestInit_ex(md5ctx, md5, NULL);
  EVP_DigestUpdate(md5ctx, text.c_str(), text.size());
  EVP_DigestFinal_ex(md5ctx, hash, &hashLength);
  EVP_MD_CTX_free(md5ctx);

  stringstream ss;

  for (int i = 0; i < hashLength; i++) {
    ss << hex << setw(2) << setfill('0') << static_cast<int>(hash[i]);
  }
  return ss.str();
}
} // namespace MHR_utils