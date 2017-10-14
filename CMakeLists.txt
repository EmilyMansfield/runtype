cmake_minimum_required(VERSION 3.0)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
	"${CMAKE_SOURCE_DIR}/cmake/Modules/")

add_compile_options(-Wall -Wextra -std=c++17)

add_library(Runtype INTERFACE)
target_include_directories(Runtype INTERFACE include)

enable_testing()
add_subdirectory(test)
add_subdirectory(libs/catch)
