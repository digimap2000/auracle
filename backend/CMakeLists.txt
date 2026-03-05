cmake_minimum_required(VERSION 3.25)
project(auracle LANGUAGES CXX)

if(APPLE)
    enable_language(OBJCXX)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(AuracleCompiler)
include(AuracleOptions)
include(AuracleDependencies)

add_subdirectory(dts-bluetooth)
add_subdirectory(assigned-numbers)
add_subdirectory(inventory)
add_subdirectory(rpc)
add_subdirectory(daemon)
add_subdirectory(cli)
