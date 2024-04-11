#pragma once

#include <ctime>
#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <iomanip>
#include <re2/re2.h>
#include <sstream>
#include <string>

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

static void log(string message) {
  time_t currentTime = time(nullptr);

  fmt::println("{} {}",
               fmt::format(fmt::fg(fmt::color::gray), "{:%Y-%m-%d %H:%M%p}",
                           fmt::localtime(currentTime)),
               message);
}

static void log(string from, string message,
                fmt::color color = fmt::color::light_sea_green) {
  log(fmt::format("{} {}", fmt::format(fmt::fg(color), from), message));
}

static string formatStringMap(vector<vector<string>> stringMap) {
  vector<string> mesgs = {};

  for (auto const &str : stringMap)
    mesgs.push_back(fmt::format(
        "{}{}", fmt::format(fmt::fg(fmt::color::light_blue), "{}=", str[0]),
        str[1]));

  return fmt::format("{}", fmt::join(mesgs, " "));
}

static void log(vector<vector<string>> stringMap) {
  log(formatStringMap(stringMap));
}

static void log(string from, vector<vector<string>> stringMap,
                fmt::color color = fmt::color::light_sea_green) {
  log(from, formatStringMap(stringMap), color);
}

static bool isSuccess(const int &code) {
  int firstDigit = code / 100;
  return firstDigit != 4 && firstDigit != 5;
}