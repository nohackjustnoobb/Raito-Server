// Source: https://github.com/CrowCpp/Crow/blob/master/include/crow/mime_types.h
/*
BSD 3-Clause License

Copyright (c) 2014-2017, ipkn
              2020-2022, CrowCpp
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the author nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The Crow logo and other graphic material (excluding third party logos) used are
under exclusive Copyright (c) 2021-2022, Farook Al-Sammarraie (The-EDev), All
rights reserved.
*/

// This file is generated from nginx/conf/mime.types using nginx_mime2cpp.py on
// 2021-12-03.
#include <string>
#include <unordered_map>

const std::unordered_map<std::string, std::string> mime_types{
    {"shtml", "text/html"},
    {"htm", "text/html"},
    {"html", "text/html"},
    {"css", "text/css"},
    {"xml", "text/xml"},
    {"gif", "image/gif"},
    {"jpg", "image/jpeg"},
    {"jpeg", "image/jpeg"},
    {"js", "application/javascript"},
    {"atom", "application/atom+xml"},
    {"rss", "application/rss+xml"},
    {"mml", "text/mathml"},
    {"txt", "text/plain"},
    {"jad", "text/vnd.sun.j2me.app-descriptor"},
    {"wml", "text/vnd.wap.wml"},
    {"htc", "text/x-component"},
    {"avif", "image/avif"},
    {"png", "image/png"},
    {"svgz", "image/svg+xml"},
    {"svg", "image/svg+xml"},
    {"tiff", "image/tiff"},
    {"tif", "image/tiff"},
    {"wbmp", "image/vnd.wap.wbmp"},
    {"webp", "image/webp"},
    {"ico", "image/x-icon"},
    {"jng", "image/x-jng"},
    {"bmp", "image/x-ms-bmp"},
    {"woff", "font/woff"},
    {"woff2", "font/woff2"},
    {"ear", "application/java-archive"},
    {"war", "application/java-archive"},
    {"jar", "application/java-archive"},
    {"json", "application/json"},
    {"hqx", "application/mac-binhex40"},
    {"doc", "application/msword"},
    {"pdf", "application/pdf"},
    {"ai", "application/postscript"},
    {"eps", "application/postscript"},
    {"ps", "application/postscript"},
    {"rtf", "application/rtf"},
    {"m3u8", "application/vnd.apple.mpegurl"},
    {"kml", "application/vnd.google-earth.kml+xml"},
    {"kmz", "application/vnd.google-earth.kmz"},
    {"xls", "application/vnd.ms-excel"},
    {"eot", "application/vnd.ms-fontobject"},
    {"ppt", "application/vnd.ms-powerpoint"},
    {"odg", "application/vnd.oasis.opendocument.graphics"},
    {"odp", "application/vnd.oasis.opendocument.presentation"},
    {"ods", "application/vnd.oasis.opendocument.spreadsheet"},
    {"odt", "application/vnd.oasis.opendocument.text"},
    {"pptx", "application/"
             "vnd.openxmlformats-officedocument.presentationml.presentation"},
    {"xlsx",
     "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {"docx",
     "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {"wmlc", "application/vnd.wap.wmlc"},
    {"wasm", "application/wasm"},
    {"7z", "application/x-7z-compressed"},
    {"cco", "application/x-cocoa"},
    {"jardiff", "application/x-java-archive-diff"},
    {"jnlp", "application/x-java-jnlp-file"},
    {"run", "application/x-makeself"},
    {"pm", "application/x-perl"},
    {"pl", "application/x-perl"},
    {"pdb", "application/x-pilot"},
    {"prc", "application/x-pilot"},
    {"rar", "application/x-rar-compressed"},
    {"rpm", "application/x-redhat-package-manager"},
    {"sea", "application/x-sea"},
    {"swf", "application/x-shockwave-flash"},
    {"sit", "application/x-stuffit"},
    {"tk", "application/x-tcl"},
    {"tcl", "application/x-tcl"},
    {"crt", "application/x-x509-ca-cert"},
    {"pem", "application/x-x509-ca-cert"},
    {"der", "application/x-x509-ca-cert"},
    {"xpi", "application/x-xpinstall"},
    {"xhtml", "application/xhtml+xml"},
    {"xspf", "application/xspf+xml"},
    {"zip", "application/zip"},
    {"dll", "application/octet-stream"},
    {"exe", "application/octet-stream"},
    {"bin", "application/octet-stream"},
    {"deb", "application/octet-stream"},
    {"dmg", "application/octet-stream"},
    {"img", "application/octet-stream"},
    {"iso", "application/octet-stream"},
    {"msm", "application/octet-stream"},
    {"msp", "application/octet-stream"},
    {"msi", "application/octet-stream"},
    {"kar", "audio/midi"},
    {"midi", "audio/midi"},
    {"mid", "audio/midi"},
    {"mp3", "audio/mpeg"},
    {"ogg", "audio/ogg"},
    {"m4a", "audio/x-m4a"},
    {"ra", "audio/x-realaudio"},
    {"3gp", "video/3gpp"},
    {"3gpp", "video/3gpp"},
    {"ts", "video/mp2t"},
    {"mp4", "video/mp4"},
    {"mpg", "video/mpeg"},
    {"mpeg", "video/mpeg"},
    {"mov", "video/quicktime"},
    {"webm", "video/webm"},
    {"flv", "video/x-flv"},
    {"m4v", "video/x-m4v"},
    {"mng", "video/x-mng"},
    {"asf", "video/x-ms-asf"},
    {"asx", "video/x-ms-asf"},
    {"wmv", "video/x-ms-wmv"},
    {"avi", "video/x-msvideo"}};
