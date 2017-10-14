add_compile_options(-O3)

# It might be better to split the tests into separate executables, one
# for each cpp file. In that case main.cpp still only needs to be built
# once, and not for each test executable, but it is hard to communicate
# this to CMake; CMake's OBJECT type cannot be used in
# target_link_libraries, and so cannot be linked with Catch.

add_executable(test_runtype
	main.cpp
	test_order_preserving_map.cpp
	test_runtype.cpp)
target_include_directories(test_runtype
	PRIVATE ${CMAKE_SOURCE_DIR}/test)
target_link_libraries(test_runtype PRIVATE Runtype Catch)

include(ParseAndAddCatchTests)
ParseAndAddCatchTests(test_runtype)
