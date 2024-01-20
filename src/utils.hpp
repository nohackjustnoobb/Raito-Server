#pragma once

#include <iomanip>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <re2/re2.h>
#include <sstream>

using namespace std;

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

template <typename T> static void releaseMemory(vector<T> vector) {
  for (T ptr : vector) {
    delete ptr;
  }
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