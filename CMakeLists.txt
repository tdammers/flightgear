cmake_minimum_required (VERSION 2.6.4)

include (CheckFunctionExists)
include (CheckCSourceCompiles)
include (CheckCXXSourceCompiles)
include (CheckIncludeFile)

project(FlightGear)

# We have some custom .cmake scripts not in the official distribution.
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/CMakeModules;${CMAKE_MODULE_PATH}")

if(${CMAKE_VERSION} VERSION_GREATER 2.8.4)
    # use official include provided by latest CMake
    include(GNUInstallDirs)
else(${CMAKE_VERSION} VERSION_GREATER 2.8.4)
    # backward compatibility: use our own module for older cmake versions
    include(OldGNUInstallDirs)
endif(${CMAKE_VERSION} VERSION_GREATER 2.8.4)

# Warning when build is not an out-of-source build.
string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}" InSourceBuild)
if(InSourceBuild)
    message(WARNING  "Avoid building inside the source tree!")
    message(WARNING  "Create a separate build directory instead (i.e. 'fgbuild') and call CMake from there: ")
    message(WARNING  "  mkdir ../fgbuild && cd ../fgbuild && cmake ${CMAKE_SOURCE_DIR}")
endif(InSourceBuild)

# System detection/default settings
include( DetectDistro )
include( DetectBrowser )

set(CMAKE_DEBUG_POSTFIX          "d" CACHE STRING "add a postfix, usually d on windows")
set(CMAKE_RELEASE_POSTFIX        ""  CACHE STRING "add a postfix, usually empty on windows")
set(CMAKE_RELWITHDEBINFO_POSTFIX ""  CACHE STRING "add a postfix, usually empty on windows")
set(CMAKE_MINSIZEREL_POSTFIX     ""  CACHE STRING "add a postfix, usually empty on windows")

# read 'version' file into a variable (stripping any newlines or spaces)
file(READ version versionFile)
if (NOT versionFile)
    message(FATAL_ERROR "Unable to determine FlightGear version. Version file is missing.")
endif()
string(STRIP "${versionFile}" FLIGHTGEAR_VERSION)

# FlightGear packaging (to build a source tarball)
include( ConfigureCPack )

# FlightGear base package path
if (FG_DATA_DIR)
    message(STATUS "Using explicit data directory for base package: ${FG_DATA_DIR}")
else()
    set(FG_DATA_DIR "${CMAKE_INSTALL_PREFIX}/lib/FlightGear" CACHE PATH "Default location where data files are located")
    message(STATUS "Using default data directory for base package:  ${FG_DATA_DIR}")
endif()

# Change the default build type to something fast
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
        FORCE)
endif(NOT CMAKE_BUILD_TYPE)

if(NOT "$ENV{BUILD_ID}" STREQUAL "")
    set(HUDSON_BUILD_ID $ENV{BUILD_ID})
    set(HUDSON_BUILD_NUMBER $ENV{BUILD_NUMBER})
    message(STATUS "running under Hudson/Jenkins, build-number is ${HUDSON_BUILD_NUMBER}")
else()
    set(HUDSON_BUILD_NUMBER 0)
    set(HUDSON_BUILD_ID "none")
endif()

#####################################################################################
# Configure library search paths
#####################################################################################

if(APPLE)
    # Custom library directories for Mac, which should have precedence over any other
    list(APPEND ADDITIONAL_LIBRARY_PATHS
         ~/Library/Frameworks
         /Library/Frameworks)
endif(APPLE)

if(NOT "${CMAKE_LIBRARY_ARCHITECTURE}" STREQUAL "")
    # Workaround for Ubuntu/Debian which introduced the "multiarch" library
    # directory structure, which is unsupported by CMake < 2.8.10, so we need to
    # add paths manually
    # see http://www.cmake.org/Bug/view.php?id=12049 and
    # http://www.cmake.org/Bug/view.php?id=12037
    list(APPEND ADDITIONAL_LIBRARY_PATHS
         /usr/local/lib/${CMAKE_LIBRARY_ARCHITECTURE}
         /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}
         /lib/${CMAKE_LIBRARY_ARCHITECTURE})
    message(STATUS "additional library directories: ${ADDITIONAL_LIBRARY_PATHS}")
endif()

if(NOT MSVC)
    # TBD: are these really necessary? Aren't they considered by cmake automatically?
    list(APPEND ADDITIONAL_LIBRARY_PATHS
         /opt/local
         /usr/local
         /usr)
endif()
#####################################################################################

IF(APPLE)
    set(EVENT_INPUT_DEFAULT 1)

    find_library(CORESERVICES_LIBRARY CoreServices)
    find_library(COCOA_LIBRARY Cocoa)
    list(APPEND PLATFORM_LIBS ${COCOA_LIBRARY} ${CORESERVICES_LIBRARY})

elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    find_package(UDev)

    if(UDEV_FOUND)
        set(EVENT_INPUT_DEFAULT 1)
    endif(UDEV_FOUND)
endif()

find_package(Git)
if (GIT_FOUND)
    execute_process(COMMAND git --git-dir ${PROJECT_SOURCE_DIR}/.git rev-parse  HEAD
        OUTPUT_VARIABLE REVISION
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    message(STATUS "Git revision is ${REVISION}")
else()
    set(REVISION "none")
endif()

# FlightGear build options
option(SIMGEAR_SHARED    "Set to ON when SimGear was built as a shared library" OFF)
option(LOGGING           "Set to ON to build FlightGear with logging support (default)" ON)
option(SP_FDMS           "Set to ON to build FlightGear with special-purpose FDMs" OFF)
option(ENABLE_UIUC_MODEL "Set to ON to build FlightGear with UIUCModel FDM" OFF)
option(ENABLE_LARCSIM    "Set to ON to build FlightGear with LaRCsim FDM" OFF)
option(ENABLE_YASIM      "Set to ON to build FlightGear with YASIM FDM (default)" ON)
option(ENABLE_JSBSIM     "Set to ON to build FlightGear with JSBSim FDM (default)" ON)
option(EVENT_INPUT       "Set to ON to build FlightGear with event-based Input support" ${EVENT_INPUT_DEFAULT})
option(ENABLE_LIBSVN     "Set to ON to build FlightGear/terrasync with libsvnclient support (default)" ON)
option(ENABLE_RTI        "Set to ON to build FlightGear with RTI support" OFF)
option(ENABLE_PROFILE    "Set to ON to build FlightGear with gperftools profiling support" OFF)
option(JPEG_FACTORY      "Set to ON to build FlightGear with JPEG-factory support" OFF)
option(SYSTEM_SQLITE     "Set to ON to build FlightGear with the system's SQLite3 library" OFF)

# additional utilities
option(ENABLE_FGADMIN    "Set to ON to build the FGADMIN application (default)" ON)
option(WITH_FGPANEL      "Set to ON to build the fgpanel application (default)" ON)
option(ENABLE_TESTS      "Set to ON to build test applications (default)" ON)

if(LOGGING)
    # nothing
else()
    set(FG_NDEBUG 1)
endif()

if(SP_FDMS)
    set(ENABLE_SP_FDM 1)
endif()

# Setup MSVC 3rd party directories
include( ConfigureMsvc3rdParty )

if(EVENT_INPUT)
    if(APPLE)
    elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
        if(NOT UDEV_FOUND)
            message(WARNING "UDev not found, event input is disabled!")
            set(EVENT_INPUT 0)
        else()
            add_definitions(-DWITH_EVENTINPUT)
            set(EVENT_INPUT_LIBRARIES ${UDEV_LIBRARIES})
            message(STATUS "event-based input enabled. Using ${UDEV_LIBRARIES}")
        endif()
    else()
        message(WARNING "event-based input is not supported on this platform yet")
    endif()

    # Keep PLIB INPUT enabled as long as EventInput does not replace current joystick configurations.
    set(ENABLE_PLIB_JOYSTICK 1)
else(EVENT_INPUT)
    set(ENABLE_PLIB_JOYSTICK 1)
endif(EVENT_INPUT)

# check required dependencies
find_package(Boost   REQUIRED)
find_package(ZLIB    REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenGL  REQUIRED)
find_package(OpenAL  REQUIRED)
find_package(OpenSceneGraph 3.0.0 REQUIRED osgText osgSim osgDB osgParticle osgFX osgUtil osgViewer osgGA)

if(ENABLE_FGADMIN)
    find_package(FLTK)

    if ( FLTK_FOUND )
        if ( X11_Xinerama_FOUND )
            message(STATUS "Found X11_Xinerama...")
            list(APPEND FLTK_LIBRARIES ${X11_Xinerama_LIB})
        endif()

        if ( X11_Xft_FOUND )
            message(STATUS "Found X11_Xft...")
            list(APPEND FLTK_LIBRARIES ${X11_Xft_LIB})
        endif()

        message(STATUS "Using FLTK_LIBRARIES for fgadmin: ${FLTK_LIBRARIES}")
    endif ( FLTK_FOUND )
endif(ENABLE_FGADMIN)

if(ENABLE_LIBSVN)
    find_package(SvnClient)

    if(LIBSVN_FOUND)
        message(STATUS "libsvn found, enabling in terrasync")
        set(HAVE_SVN_CLIENT_H 1)
        set(HAVE_LIBSVN_CLIENT_1 1)
    else()
        message(STATUS "libsvn missing. Disabling...")
    endif(LIBSVN_FOUND)
endif(ENABLE_LIBSVN)

if (SYSTEM_SQLITE)
    find_package(SQLite3 REQUIRED)
    set(CMAKE_REQUIRED_INCLUDES ${SQLITE3_INCLUDED_DIR})
    message(STATUS "Using system SQLite3 library")
endif (SYSTEM_SQLITE)

# Sqlite always depends on the threading lib
list(APPEND SQLITE3_LIBRARY ${CMAKE_THREAD_LIBS_INIT})

find_package(PLIB REQUIRED puaux pu js fnt)

# FlightGear and SimGear versions need to match
find_package(SimGear ${FLIGHTGEAR_VERSION} REQUIRED)

if (JPEG_FACTORY)
    # check simgear was built with JPEG-factory support
    find_package(JPEG REQUIRED)
    include_directories(${JPEG_INCLUDE_DIR})

    set(CMAKE_REQUIRED_INCLUDES
        ${SIMGEAR_INCLUDE_DIR}
        ${JPEG_INCLUDE_DIR}
        ${OPENSCENEGRAPH_INCLUDE_DIRS})

    check_cxx_source_compiles(
        "#include <simgear/screen/jpgfactory.hxx>
        int main()    { return 0; } "
        FG_JPEG_SERVER)

    if (NOT FG_JPEG_SERVER)
        message(STATUS "JPEG server support requested, but SimGear was built without JPEG support")
    endif()
endif()

check_include_file(unistd.h   HAVE_UNISTD_H)
check_include_file(sys/time.h HAVE_SYS_TIME_H)
check_include_file(windows.h  HAVE_WINDOWS_H)

if(ENABLE_PROFILE)
    find_package(GooglePerfTools REQUIRED)
    set(FG_HAVE_GPERFTOOLS 1)
    message(STATUS "Built-in profiler using gperftools available")
endif()

if(ENABLE_RTI)
    find_package(RTI)
    if(RTI_FOUND)
        set(FG_HAVE_HLA 1)
    endif(RTI_FOUND)
endif(ENABLE_RTI)

if(CMAKE_COMPILER_IS_GNUCXX)
    set(WARNING_FLAGS_CXX "-Wall")
    set(WARNING_FLAGS_C   "-Wall")
endif(CMAKE_COMPILER_IS_GNUCXX)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang") 
   set (WARNING_FLAGS_CXX "-Wall -Wno-overloaded-virtual")
   set (WARNING_FLAGS_C   "-Wall")
endif() 

if(WIN32)

    if(MSVC)
        # turn off various warnings
        # foreach(warning 4244 4251 4267 4275 4290 4786 4305 4996)
        #     SET(WARNING_FLAGS "${WARNING_FLAGS} /wd${warning}")
        # endforeach(warning)

        set(MSVC_FLAGS "-DNOMINMAX -D_USE_MATH_DEFINES -D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS -D__CRT_NONSTDC_NO_WARNINGS")
        if (${MSVC_VERSION} GREATER 1599)
            set( MSVC_LD_FLAGS "/FORCE:MULTIPLE" )
        endif (${MSVC_VERSION} GREATER 1599)
    endif(MSVC)

    set(NOMINMAX 1)
endif(WIN32)

set (BOOST_CXX_FLAGS "-DBOOST_MULTI_INDEX_DISABLE_SERIALIZATION -DBOOST_BIMAP_DISABLE_SERIALIZATION")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WARNING_FLAGS_C} ${MSVC_FLAGS} -D_REENTRANT")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNING_FLAGS_CXX} ${MSVC_FLAGS} -D_REENTRANT ${BOOST_CXX_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${MSVC_LD_FLAGS}")

include_directories(${OPENSCENEGRAPH_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIR}
    ${OPENGL_INCLUDE_DIR}
    ${OPENAL_INCLUDE_DIR}
    ${SIMGEAR_INCLUDE_DIR}
    ${PLIB_INCLUDE_DIR} )

include_directories(${PROJECT_SOURCE_DIR}/src)

# following is needed, because config.h is include 'bare', whereas
# version.h is included as <Include/version.h> - this should be cleaned up
include_directories(${PROJECT_BINARY_DIR}/src)
include_directories(${PROJECT_BINARY_DIR}/src/Include)

add_definitions(-DHAVE_CONFIG_H)

check_function_exists(mkfifo HAVE_MKFIFO)

# configure a header file to pass some of the CMake settings
# to the source code
configure_file (
    "${PROJECT_SOURCE_DIR}/src/Include/config_cmake.h.in"
    "${PROJECT_BINARY_DIR}/src/Include/config.h"
)

#and the same for the version header
configure_file (
    "${PROJECT_SOURCE_DIR}/src/Include/version.h.cmake-in"
    "${PROJECT_BINARY_DIR}/src/Include/version.h"
)

add_subdirectory(src)
add_subdirectory(utils)
add_subdirectory(man)

#-----------------------------------------------------------------------------
### uninstall target
#-----------------------------------------------------------------------------
CONFIGURE_FILE(
    "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)
ADD_CUSTOM_TARGET(uninstall
    "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
