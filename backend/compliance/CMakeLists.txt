add_library(auracle_compliance
  src/engine.cpp
  src/evaluator.cpp
  src/loader.cpp
  src/normalizer.cpp
  src/parser.cpp
  src/repository.cpp
  src/suite_loader.cpp
  src/tokenizer.cpp
)

target_include_directories(auracle_compliance
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PUBLIC ${PROJECT_SOURCE_DIR}/observation/include
)

target_compile_features(auracle_compliance PUBLIC cxx_std_23)
target_compile_definitions(auracle_compliance
  PUBLIC
    AURACLE_COMPLIANCE_RULE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/rules"
    AURACLE_COMPLIANCE_SUITE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/suites"
)

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
    tests/engine_test.cpp
    tests/evaluator_test.cpp
    tests/loader_test.cpp
    tests/normalizer_test.cpp
    tests/parser_test.cpp
    tests/repository_test.cpp
    tests/suite_loader_test.cpp
  )

  target_link_libraries(auracle_compliance_tests
    PRIVATE auracle_compliance GTest::gtest_main
  )

  include(GoogleTest)
  gtest_discover_tests(auracle_compliance_tests)
endif()
