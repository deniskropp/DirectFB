include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
include_directories ("${PROJECT_BINARY_DIR}/proxy/dispatcher")
include_directories ("${PROJECT_SOURCE_DIR}/proxy/dispatcher")


DEFINE_REQUESTOR_MODULE (IDirectFBDataBuffer)
DEFINE_REQUESTOR_MODULE (IDirectFB)
DEFINE_REQUESTOR_MODULE (IDirectFBDisplayLayer)
DEFINE_REQUESTOR_MODULE (IDirectFBEventBuffer)
DEFINE_REQUESTOR_MODULE (IDirectFBFont)
DEFINE_REQUESTOR_MODULE (IDirectFBImageProvider)
DEFINE_REQUESTOR_MODULE (IDirectFBInputDevice)
DEFINE_REQUESTOR_MODULE (IDirectFBPalette)
DEFINE_REQUESTOR_MODULE (IDirectFBScreen)
DEFINE_REQUESTOR_MODULE (IDirectFBSurface)
DEFINE_REQUESTOR_MODULE (IDirectFBWindow)

if (ENABLE_FUSIONDALE)
        include_directories ("${PROJECT_BINARY_DIR}/lib/fusiondale")
        include_directories ("${PROJECT_SOURCE_DIR}/lib/fusiondale")

	DEFINE_REQUESTOR_MODULE (IFusionDale)
	DEFINE_REQUESTOR_MODULE (IComaComponent)
	DEFINE_REQUESTOR_MODULE (IComa)
endif()

if (ENABLE_DIVINE)
        include_directories ("${PROJECT_BINARY_DIR}/lib/divine")
        include_directories ("${PROJECT_SOURCE_DIR}/lib/divine")

	DEFINE_REQUESTOR_MODULE (IDivine)
endif()

if (ENABLE_FUSIONSOUND)
        include_directories ("${PROJECT_BINARY_DIR}/lib/fusionsound")
        include_directories ("${PROJECT_SOURCE_DIR}/lib/fusionsound")

	DEFINE_REQUESTOR_MODULE (IFusionSoundBuffer)
	DEFINE_REQUESTOR_MODULE (IFusionSound)
	DEFINE_REQUESTOR_MODULE (IFusionSoundPlayBack)
	DEFINE_REQUESTOR_MODULE (IFusionSoundStream)
endif()

