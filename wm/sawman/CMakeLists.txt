include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories ("${PROJECT_BINARY_DIR}/lib/sawman")
include_directories ("${PROJECT_SOURCE_DIR}/lib/sawman")

DEFINE_DIRECTFB_MODULE (wm_sawman directfbwm_sawman sawman_wm.c sawman ${WM_DIR})
