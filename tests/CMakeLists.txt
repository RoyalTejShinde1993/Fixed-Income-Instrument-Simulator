enable_testing()

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

add_executable(fixed_income_tests
    bond_test.cpp
    yield_curve_test.cpp
    fix_test.cpp
    network_test.cpp
    risk_test.cpp
)

target_link_libraries(fixed_income_tests
    GTest::gtest_main
    fixed_income
)

target_include_directories(fixed_income_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

gtest_discover_tests(fixed_income_tests)
