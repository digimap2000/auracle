cmake_minimum_required(VERSION 3.25)
project(auracle LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(AuracleCompiler)
include(AuracleOptions)
include(AuracleDependencies)

add_subdirectory(inventory)
add_subdirectory(rpc)
add_subdirectory(daemon)
add_subdirectory(cli)
