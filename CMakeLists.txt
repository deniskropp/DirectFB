cmake_minimum_required (VERSION 2.8)
project (DirectFB C CXX)

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
	message(STATUS "Build type not specified: defaulting to Release.")
endif()

set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include (CheckSymbolExists)
include (CheckTypeSize)
include (CheckCSourceCompiles)
include (DirectFBFunctions)

set (LINUX 1)

set (DIRECTFB_MAJOR_VERSION 1)
set (DIRECTFB_MINOR_VERSION 7)
set (DIRECTFB_MICRO_VERSION 0)
set (DIRECTFB_BINARY_AGE    0)
set (DIRECTFB_INTERFACE_AGE 0)

set (DIRECTFB_VERSION "${DIRECTFB_MAJOR_VERSION}.${DIRECTFB_MINOR_VERSION}.${DIRECTFB_MICRO_VERSION}")
set (LIBVER "${DIRECTFB_MAJOR_VERSION}.${DIRECTFB_MINOR_VERSION}")


option (ENABLE_DEBUG "enable debug support" OFF)
option (ENABLE_TRACE "enable trace support" OFF)
option (ENABLE_MULTI "enable multi application support" OFF)

option (ENABLE_VOODOO "enable voodoo support" OFF)
option (ENABLE_ONE "enable One support" OFF)
option (ENABLE_SAWMAN "enable SaWMan support" OFF)
option (ENABLE_FUSIONDALE "enable Fusiondale support" OFF)

option (ENABLE_FONTPROVIDER_DGIFF "enable dgiff font support" ON)

if (LINUX)
	option (ENABLE_SYSTEM_FBDEV "enable fbdev support" ON)
endif()

find_package (X11)
if (X11_FOUND)
	option (ENABLE_SYSTEM_X11 "enable X11 system" ON)
else()
	message ("Warning: X11 could not be found. Building without X11 support")
endif()

find_package (Freetype)
if (FREETYPE_FOUND)
	option (ENABLE_FONTPROVIDER_FREETYPE "enable freetype font provider" ON)
else()
	message ("Warning: freetype could not be found. Building without freetype support")
endif()

find_package (PNG)
if (PNG_FOUND)
	option (ENABLE_IMAGEPROVIDER_PNG "enable png image provider" ON)
else()
	message ("Warning: libpng could not be found. Building without png support")
endif()

find_package (JPEG)
if (JPEG_FOUND)
	option (ENABLE_IMAGEPROVIDER_JPEG "enable jpeg image provider" ON)
else()
	message ("Warning: libjpeg could not be found. Building without jpeg support")
endif() 

if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    SET(ARCH_X86_64 1)
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "x86|i686")
    SET(ARCH_X86 1)
endif()

set (MKNAMES  "${PROJECT_SOURCE_DIR}/tools/mknames.sh")
set (MKRESULT "${PROJECT_SOURCE_DIR}/tools/mkresult.sh")
set (FLUXCOMP "fluxcomp")
set (FLUXCOMP_OPTIONS -i --call-mode --static-args-bytes=1024 --dispatch-error-abort)

add_definitions (-DFLUXED_ARGS_BYTES=1024)

execute_process (COMMAND date -u "+%Y-%m-%d %H:%M" OUTPUT_VARIABLE BUILDTIME OUTPUT_STRIP_TRAILING_WHITESPACE)

add_definitions (-DBUILDTIME="${BUILDTIME}")
add_definitions (-DDATADIR="${CMAKE_INSTALL_PREFIX}/share/directfb-${DIRECTFB_VERSION}")
add_definitions (-DMODULEDIR="${CMAKE_INSTALL_PREFIX}/lib/directfb-${LIBVER}-0")

set (SYSTEMS_DIR "${CMAKE_INSTALL_PREFIX}/lib/directfb-${LIBVER}-0/systems" )
set (WM_DIR "${CMAKE_INSTALL_PREFIX}/lib/directfb-${LIBVER}-0/wm" )
set (INTERFACES_DIR "${CMAKE_INSTALL_PREFIX}/lib/directfb-${LIBVER}-0/interfaces" )
set (FONTPROVIDER_DIR "${INTERFACES_DIR}/IDirectFBFont" )
set (IMAGEPROVIDER_DIR "${INTERFACES_DIR}/IDirectFBImageProvider" )

# Special exception if prefix is /usr so we don't make a /usr/etc.
string(COMPARE EQUAL ${CMAKE_INSTALL_PREFIX} "/usr" isusr)
if(isusr)
	set(SYSCONFDIR          "/etc" CACHE PATH "System configuration directory")
else(isusr)
	set(SYSCONFDIR          "${CMAKE_INSTALL_PREFIX}/etc" CACHE PATH "System configuration directory")
endif(isusr)

add_definitions (-DSYSCONFDIR="${SYSCONFDIR}")
add_definitions (-D_GNU_SOURCE=1)

if (ENABLE_ONE)
	set (DIRECTFB_BUILD_ONE 1)
	set (DEP_ONE one)
else()
	set (DIRECTFB_BUILD_ONE 0)
endif()

if (ENABLE_VOODOO)
	set (DIRECTFB_BUILD_VOODOO 1)
	set (DIRECTFB_BUILD_PURE_VOODOO 0)
	set (DEP_VOODOO voodoo)
else()
	set (DIRECTFB_BUILD_VOODOO 0)
	set (DIRECTFB_BUILD_PURE_VOODOO 0)
endif()

if (ENABLE_DEBUG)
	set (DIRECT_BUILD_DEBUG 1)
	set (DIRECT_BUILD_DEBUGS 1)
else()
	set (DIRECT_BUILD_DEBUG 0)
	set (DIRECT_BUILD_DEBUGS 0)
endif()

if (ENABLE_TRACE)
	set (DIRECT_BUILD_TRACE 1)
else()
	set (DIRECT_BUILD_TRACE 0)
endif()

set (DIRECT_BUILD_TEXT 1)
set (DIRECT_BUILD_GETTID 1)
set (DIRECT_BUILD_NETWORK 1)
set (DIRECT_BUILD_STDBOOL 1)
set (DIRECT_BUILD_DYNLOAD 1)
set (DIRECT_BUILD_MULTICORE 1)
set (DIRECT_BUILD_OSTYPE DIRECT_OS_LINUX_GNU_LIBC)
set (DIRECT_BUILD_GCC_ATOMICS 1)

if (ENABLE_MULTI)
	set (FUSION_BUILD_MULTI 1)
	set (FUSION_BUILD_KERNEL 1)
else()
	set (FUSION_BUILD_MULTI 0)
	set (FUSION_BUILD_KERNEL 0)
endif()

set (FUSION_MESSAGE_SIZE 16384)

CHECK_PTHREADS()

# .pc

set (THREADFLAGS ${PTHREAD_CFLAGS})
set (THREADLIB ${PTHREAD_LDFLAGS})
set (DYNLIB "-ldl")
set (LIBM "-lm")
set (prefix ${CMAKE_INSTALL_PREFIX})
set (libdir "\${exec_prefix}/lib")
set (exec_prefix "\${prefix}")
set (includedir "\${prefix}/include")
set (INCLUDEDIR "\${prefix}/include/directfb")
set (GENERIC_INCLUDEDIR "\${prefix}/include")
set (VERSION "${DIRECTFB_VERSION}")
set (MODULEDIR "\${exec_prefix}/lib/directfb-${LIBVER}-0")
set (MODULEDIRNAME "directfb-${LIBVER}-0")

configure_file ("${PROJECT_SOURCE_DIR}/directfb.pc.in"
                "${PROJECT_BINARY_DIR}/directfb.pc" @ONLY )

configure_file ("${PROJECT_SOURCE_DIR}/cmake/config.h.in"
                "${PROJECT_BINARY_DIR}/config.h" )

configure_file ("${PROJECT_SOURCE_DIR}/include/directfb_build.h.in"
                "${PROJECT_BINARY_DIR}/include/directfb_build.h" )

configure_file ("${PROJECT_SOURCE_DIR}/include/directfb_version.h.in"
                "${PROJECT_BINARY_DIR}/include/directfb_version.h" )

include_directories ("${PROJECT_BINARY_DIR}")
include_directories ("${PROJECT_BINARY_DIR}/include")
include_directories ("${PROJECT_BINARY_DIR}/lib")
include_directories ("${PROJECT_BINARY_DIR}/src")
include_directories ("${PROJECT_SOURCE_DIR}")
include_directories ("${PROJECT_SOURCE_DIR}/include")
include_directories ("${PROJECT_SOURCE_DIR}/lib")
include_directories ("${PROJECT_SOURCE_DIR}/src")

set (CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} ${THREADFLAGS}")
set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${THREADFLAGS}")

add_subdirectory (include)
add_subdirectory (lib/direct)
add_subdirectory (lib/fusion)

if (ENABLE_VOODOO)
	add_subdirectory (lib/voodoo)
endif()
if (ENABLE_ONE)
	add_subdirectory (lib/One)
endif()
if (ENABLE_SAWMAN)
	add_subdirectory (lib/sawman)
endif()
if (ENABLE_FUSIONDALE)
	add_subdirectory (lib/fusiondale)
endif()

add_subdirectory (src)
add_subdirectory (wm/default)
if (ENABLE_SAWMAN)
	add_subdirectory (wm/sawman)
endif()

add_subdirectory (interfaces/IDirectFBImageProvider)
add_subdirectory (interfaces/IDirectFBFont)

if (ENABLE_SYSTEM_FBDEV)
	add_subdirectory (systems/fbdev)
endif()

if (ENABLE_SYSTEM_X11)
	add_subdirectory (systems/x11)
endif()


install(
	FILES ${CMAKE_CURRENT_BINARY_DIR}/directfb.pc
	DESTINATION lib/pkgconfig
)
