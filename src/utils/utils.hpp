#pragma once

#include <iomanip>
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
