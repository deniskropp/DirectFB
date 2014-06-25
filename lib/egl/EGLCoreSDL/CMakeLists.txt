include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
include_directories ("${PROJECT_BINARY_DIR}/lib/sdl")
include_directories ("${PROJECT_SOURCE_DIR}/lib/sdl")
include_directories ("${SDL_INCLUDE_DIR}")


set (DFBEGLCORESDL_LIBS
	directfb
	EGL
)

DEFINE_DIRECTFB_MODULE( dfbegl_core_sdl
	dfbegl_core_sdl EGLDisplaySDL.cpp "${DFBEGLCORESDL_LIBS}" ${MODULEDIR}/dfbegl_core
)
