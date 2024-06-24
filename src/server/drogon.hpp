#pragma once

#include <string>

using namespace std;

// Main entry point for drogon server
void startDrogonServer(int port, string *_webpageUrl, string *_accessKey,
                       string *_adminAccessKey, bool _adminAllowOnlyLocal);