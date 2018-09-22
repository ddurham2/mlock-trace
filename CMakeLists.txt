cmake_minimum_required (VERSION 2.8.11)
project (mlock-trace)


set (CMAKE_CXX_STANDARD 11)
#set (CMAKE_VERBOSE_MAKEFILE ON)

find_package (Threads)

add_library (mlock-trace SHARED mlock-trace.cpp)
target_link_libraries(mlock-trace ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS})




# === Testing =======================

add_executable(test-mlock-trace EXCLUDE_FROM_ALL test-mlock-trace.cpp)
target_compile_options(test-mlock-trace PUBLIC -g)
add_dependencies(test-mlock-trace mlock-trace)

add_custom_target(run DEPENDS test-output)
add_dependencies(run test-mlock-trace)
add_custom_command(OUTPUT test-output COMMAND 
	LD_PRELOAD=${CMAKE_CURRENT_BINARY_DIR}/libmlock-trace.so ./test-mlock-trace
DEPENDS always_rebuild)
add_custom_command(OUTPUT always_rebuild COMMAND cmake -E echo)
