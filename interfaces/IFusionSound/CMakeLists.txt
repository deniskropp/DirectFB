include_directories ("${PROJECT_BINARY_DIR}/lib/fusionsound")
include_directories ("${PROJECT_SOURCE_DIR}/lib/fusionsound")
include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")

DEFINE_DIRECTFB_MODULE (ifusionsound
	ifusionsound ifusionsound_module.c fusionsound ${INTERFACES_DIR}/IFusionSound
)
