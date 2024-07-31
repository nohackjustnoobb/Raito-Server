#pragma once

#include <ctime>
#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <iomanip>
#include <random>
#include <re2/re2.h>
#include <sstream>
#include <string>

using namespace std;

// Split the input string at each matching regex.
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

// Release memory for each element in a vector.
template <typename T> static void releaseMemory(vector<T> vector) {
  for (T ptr : vector) {
    if (ptr != nullptr)
      delete ptr;
  }
}

// Log a message
static void log(string message) {
  time_t currentTime = time(nullptr);

  fmt::println("{} {}",
               fmt::format(fmt::fg(fmt::color::gray), "{:%Y-%m-%d %H:%M%p}",
                           fmt::localtime(currentTime)),
               message);
}

// Log a message with the specified origin and its color values.
static void log(string from, string message,
                fmt::color color = fmt::color::light_sea_green) {
  log(fmt::format("{} {}", fmt::format(fmt::fg(color), from), message));
}

// This function should be called directly.
static string formatStringMap(vector<vector<string>> stringMap) {
  vector<string> mesgs = {};

  for (auto const &str : stringMap)
    mesgs.push_back(fmt::format(
        "{}{}", fmt::format(fmt::fg(fmt::color::light_blue), "{}=", str[0]),
        str[1]));

  return fmt::format("{}", fmt::join(mesgs, " "));
}

// Log messages with a title.
static void log(vector<vector<string>> stringMap) {
  log(formatStringMap(stringMap));
}

// Log messages with a title, the specified origin, and its color values.
static void log(string from, vector<vector<string>> stringMap,
                fmt::color color = fmt::color::light_sea_green) {
  log(from, formatStringMap(stringMap), color);
}

// Determine if a HTTP status code is successful
static bool isSuccess(const int &code) {
  int firstDigit = code / 100;
  return firstDigit != 4 && firstDigit != 5;
}

// Determine if a ip address is local ip
static bool isLocalIp(const string &ip) {
  return RE2::FullMatch(
      ip,
      R"((localhost|10\.([0-9]{1,3}\.){2}[0-9]{1,3}|172\.(1[6-9]|2[0-9]|3[0-1])\.([0-9]{1,3}\.)[0-9]{1,3}|192\.168\.([0-9]{1,3}\.)[0-9]{1,3}|127\.([0-9]{1,3}\.){2}[0-9]{1,3}):?\d*$)");
}

static string randomString(size_t length) {
  auto randchar = []() -> char {
    const char charset[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    const size_t max_index = (sizeof(charset) - 1);
    return charset[rand() % max_index];
  };

  string str(length, 0);
  generate_n(str.begin(), length, randchar);

  return str;
}