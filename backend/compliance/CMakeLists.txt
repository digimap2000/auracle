add_library(auracle_compliance
  src/evaluator.cpp
  src/parser.cpp
  src/tokenizer.cpp
)

target_include_directories(auracle_compliance
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(auracle_compliance PUBLIC cxx_std_23)

if(BUILD_TESTING)
  find_package(GTest CONFIG QUIET)

  if(NOT GTest_FOUND)
    include(FetchContent)

    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()

  add_executable(auracle_compliance_tests
    tests/evaluator_test.cpp
    tests/parser_test.cpp
  )

  target_link_libraries(auracle_compliance_tests
    PRIVATE auracle_compliance GTest::gtest_main
  )

  include(GoogleTest)
  gtest_discover_tests(auracle_compliance_tests)
endif()
