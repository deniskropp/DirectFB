include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories ("${PROJECT_BINARY_DIR}/lib/fusionsound")
include_directories ("${PROJECT_SOURCE_DIR}/lib/fusionsound")

add_library (dpack STATIC dpack.c)
set_target_properties( dpack PROPERTIES COMPILE_FLAGS "-fPIC") 
