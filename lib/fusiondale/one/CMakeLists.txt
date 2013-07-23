include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories ("${CMAKE_CURRENT_BINARY_DIR}")

set (FUSIONDALE_ONE_LIBS
	pthread
	dl
	rt
	direct
	fusion
	fusiondale
	one
)

DEFINE_DIRECTFB_MODULE (ifusiondale_one
	ifusiondale_one ifusiondale_one.c "${FUSIONDALE_ONE_LIBS}" ${INTERFACES_DIR}/IFusionDale
)
DEFINE_DIRECTFB_MODULE (ifusiondalemessenger_one
	ifusiondalemessenger_one ifusiondalemessenger_one.c "${FUSIONDALE_ONE_LIBS}" ${INTERFACES_DIR}/IFusionDaleMessenger
)
DEFINE_DIRECTFB_MODULE (icoma_one
	icoma_one icoma_one.c "${FUSIONDALE_ONE_LIBS}" ${INTERFACES_DIR}/IComa
)
DEFINE_DIRECTFB_MODULE (icomacomponent_one
	icomacomponent_one icomacomponent_one.c "${FUSIONDALE_ONE_LIBS}" ${INTERFACES_DIR}/IComaComponent
)
