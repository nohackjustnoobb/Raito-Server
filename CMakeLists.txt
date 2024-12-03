cmake_minimum_required(VERSION 3.12)

project(Raito-Server)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCE_FILES
    src/*.cc
    src/*.h
    src/*.hpp)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

find_package(Drogon CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(hash-library REQUIRED)
find_package(fmt REQUIRED CONFIG)
find_package(cpr CONFIG REQUIRED)
find_package(re2 REQUIRED)
find_package(freeimage REQUIRED)
find_package(SOCI REQUIRED CONFIG)
find_package(rapidfuzz REQUIRED)
find_package(lexbor REQUIRED)
find_package(libzippp REQUIRED)

target_link_libraries(${PROJECT_NAME} PUBLIC
    Drogon::Drogon
    nlohmann_json::nlohmann_json
    re2::re2
    hash-library::hash-library
    fmt::fmt
    cpr::cpr
    freeimage::freeimage
    soci::soci
    rapidfuzz::rapidfuzz
    lexbor::lexbor_static
    libzippp::libzippp
)
