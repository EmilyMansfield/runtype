cmake_minimum_required(VERSION 3.0)

add_compile_options(-Wall -Wextra -std=c++17)

add_library(Runtype INTERFACE)
target_include_directories(Runtype INTERFACE include)

add_subdirectory(test)
add_subdirectory(libs/catch)

enable_testing()
add_test(TestOrderPreservingMap test/test_order_preserving_map)
add_test(TestRuntype test/test_runtype)
