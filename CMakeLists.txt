cmake_minimum_required(VERSION 3.13)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "-Wall -pedantic -ggdb3")

find_package(PkgConfig REQUIRED)

pkg_check_modules(ILMBASE REQUIRED IMPORTED_TARGET IlmBase)

add_executable (lol2dae AnmImporter.cpp ColladaWriter.cpp EulerUtilities.cpp lol2dae.cpp SklImporter.cpp SknImporter.cpp)

target_link_libraries (lol2dae PUBLIC PkgConfig::ILMBASE)
