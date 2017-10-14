add_compile_options(-O3)

add_executable(test_order_preserving_map
	main.cpp
	test_order_preserving_map.cpp)
target_include_directories(test_order_preserving_map
	PRIVATE ${CMAKE_SOURCE_DIR}/test)
target_link_libraries(test_order_preserving_map PRIVATE Runtype Catch)

add_executable(test_runtype
	main.cpp
	test_runtype)
target_include_directories(test_runtype
	PRIVATE ${CMAKE_SOURCE_DIR}/test)
target_link_libraries(test_runtype PRIVATE Runtype Catch)
