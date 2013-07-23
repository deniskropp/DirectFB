include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories ("${CMAKE_CURRENT_BINARY_DIR}")

DEFINE_DIRECTFB_MODULE (wm_default directfbwm_default default.c directfb ${WM_DIR})
