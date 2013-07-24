include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
include_directories ("${PROJECT_BINARY_DIR}/proxy/requestor")
include_directories ("${PROJECT_SOURCE_DIR}/proxy/requestor")


DEFINE_DISPATCHER_MODULE (IDirectFBDataBuffer)
DEFINE_DISPATCHER_MODULE (IDirectFB)
DEFINE_DISPATCHER_MODULE (IDirectFBDisplayLayer)
DEFINE_DISPATCHER_MODULE (IDirectFBEventBuffer)
DEFINE_DISPATCHER_MODULE (IDirectFBFont)
DEFINE_DISPATCHER_MODULE (IDirectFBImageProvider)
DEFINE_DISPATCHER_MODULE (IDirectFBInputDevice)
DEFINE_DISPATCHER_MODULE (IDirectFBPalette)
DEFINE_DISPATCHER_MODULE (IDirectFBScreen)
DEFINE_DISPATCHER_MODULE (IDirectFBSurface)
DEFINE_DISPATCHER_MODULE (IDirectFBWindow)

if (ENABLE_FUSIONDALE)
        include_directories ("${PROJECT_BINARY_DIR}/lib/fusiondale")
        include_directories ("${PROJECT_SOURCE_DIR}/lib/fusiondale")

	DEFINE_DISPATCHER_MODULE (IFusionDale)
	DEFINE_DISPATCHER_MODULE (IComaComponent)
	DEFINE_DISPATCHER_MODULE (IComa)
endif()

if (ENABLE_DIVINE)
        include_directories ("${PROJECT_BINARY_DIR}/lib/divine")
        include_directories ("${PROJECT_SOURCE_DIR}/lib/divine")

	DEFINE_DISPATCHER_MODULE (IDivine)
endif()

if (ENABLE_FUSIONSOUND)
        include_directories ("${PROJECT_BINARY_DIR}/lib/fusionsound")
        include_directories ("${PROJECT_SOURCE_DIR}/lib/fusionsound")

	DEFINE_DISPATCHER_MODULE (IFusionSoundBuffer)
	DEFINE_DISPATCHER_MODULE (IFusionSound)
	DEFINE_DISPATCHER_MODULE (IFusionSoundPlayBack)
	DEFINE_DISPATCHER_MODULE (IFusionSoundStream)
endif()

