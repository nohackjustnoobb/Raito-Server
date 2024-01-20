cmake_minimum_required(VERSION 3.12)

project(Raito-Server)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCE_FILES
    src/*.cc
    src/*.h
    src/*.hpp)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

find_package(Crow REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(fmt REQUIRED CONFIG)
find_package(cpr CONFIG REQUIRED)
find_package(re2 REQUIRED)

# dependencies for DM5
find_package(lexbor REQUIRED)

# dependencies for MHG
find_package(SQLiteCpp REQUIRED)

target_link_libraries(${PROJECT_NAME} PUBLIC
    nlohmann_json::nlohmann_json
    Crow::Crow
    re2::re2
    OpenSSL::Crypto OpenSSL::SSL
    fmt::fmt
    cpr::cpr

    # dependencies for DM5
    lexbor::lexbor_static

    # dependencies for MHG
    SQLiteCpp
)
