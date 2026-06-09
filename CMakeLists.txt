cmake_minimum_required(VERSION 3.24)
project(FixedIncomeInstrumentSimulator VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Boost 1.81 REQUIRED COMPONENTS system)

add_library(fixed_income
    src/bond.cpp
    src/yield_curve.cpp
    src/fix.cpp
    src/network.cpp
    src/risk.cpp
)

target_include_directories(fixed_income PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
)

target_compile_options(fixed_income PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall;-Wextra;-Wpedantic>
    $<$<CXX_COMPILER_ID:Clang>:-Wall;-Wextra;-Wpedantic>
)

target_link_libraries(fixed_income PUBLIC
    Boost::system
)

option(ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
if(ENABLE_SANITIZERS)
    target_compile_options(fixed_income PRIVATE -fsanitize=address,undefined)
    target_link_libraries(fixed_income PUBLIC -fsanitize=address,undefined)
endif()

add_executable(fixed_income_demo
    src/main.cpp
)

target_link_libraries(fixed_income_demo PRIVATE fixed_income)

target_include_directories(fixed_income_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

enable_testing()
add_subdirectory(tests)
