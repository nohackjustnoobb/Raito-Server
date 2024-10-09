#pragma once

#include <fmt/chrono.h>
#include <fmt/color.h>

using namespace std;

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

// This function should not be called directly.
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
