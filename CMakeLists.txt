#
# Copyright (C) 2017-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.2.0 FATAL_ERROR)
include(ExternalProject)

# Include custom configs
file(GLOB custom_configs *custom_config.cmake)
foreach(config ${custom_configs})
    include(${config})
endforeach()

# Include needed macros
include(common_macros.cmake)

if(MSVC)
set(CMAKE_GENERATOR_TOOLSET "host=x64")
endif()

if(NOT PROJECT_NAME)
    set(PROJECT_NAME igdrcl)
endif()


project(${PROJECT_NAME})

if(CMAKE_CONFIGURATION_TYPES)
    # Set custom build types for multi-configuration generators
    set(CMAKE_CONFIGURATION_TYPES "Release;ReleaseInternal;Debug")
    if(DEFINED CMAKE_BUILD_TYPE)
        list(APPEND CMAKE_CONFIGURATION_TYPES ${CMAKE_BUILD_TYPE})
    endif()
    list(REMOVE_DUPLICATES CMAKE_CONFIGURATION_TYPES)
endif()

include(GNUInstallDirs)

# define global property where we will collect component list to package
define_property(GLOBAL PROPERTY NEO_COMPONENTS_LIST BRIEF_DOCS "List of components" FULL_DOCS "List of components to create packages")
set(GLOBAL PROPERTY NEO_COMPONENTS_LIST "")
if(NOT DEFINED NEO_BUILD_PACKAGE)
    set(NEO_BUILD_PACKAGE TRUE)
endif()

set(BRANCH_TYPE "")
set(BRANCH_DIR_SUFFIX "/${BRANCH_TYPE}")

if(TR_DEPRECATED)
    add_definitions(-D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING=1)
endif(TR_DEPRECATED)

if(RELEASE_WITH_REGKEYS)
    add_definitions(-D_RELEASE_BUILD_WITH_REGKEYS)
endif(RELEASE_WITH_REGKEYS)

if(DEFINED NEO_EXTRA_DEFINES)
    string(REPLACE "," ";" NEO_EXTRA_DEFINES "${NEO_EXTRA_DEFINES}")
    foreach(_tmp ${NEO_EXTRA_DEFINES})
        message(STATUS "adding extra define: ${_tmp}")
        add_definitions(-D${_tmp})
    endforeach()
endif()

if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "CMAKE_BUILD_TYPE not specified, using Release")
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type: [Release, ReleaseInternal, Debug]" FORCE)
endif()

add_definitions(-DCL_USE_DEPRECATED_OPENCL_1_1_APIS -DCL_USE_DEPRECATED_OPENCL_1_2_APIS -DCL_USE_DEPRECATED_OPENCL_2_0_APIS)

if(WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -DWDDM_VERSION_NUMBER=23 -DNOMINMAX)
endif()

set(CMAKE_C_FLAGS_RELEASEINTERNAL "${CMAKE_C_FLAGS_RELEASE}")
set(CMAKE_CXX_FLAGS_RELEASEINTERNAL "${CMAKE_CXX_FLAGS_RELEASE}")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASEINTERNAL "${CMAKE_SHARED_LINKER_FLAGS_RELEASE}")
set(CMAKE_EXE_LINKER_FLAGS_RELEASEINTERNAL "${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
if(MSVC AND ENABLE_VS_FASTLINK)
    string(APPEND CMAKE_EXE_LINKER_FLAGS_DEBUG " /debug:FASTLINK")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS_DEBUG " /debug:FASTLINK")
    string(APPEND CMAKE_STATIC_LINKER_FLAGS_DEBUG " /debug:FASTLINK")
    string(APPEND CMAKE_MODULE_LINKER_FLAGS_DEBUG " /debug:FASTLINK")
endif()

string(TOLOWER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_lower)
if("${BUILD_TYPE_lower}" STREQUAL "releaseinternal")
  add_definitions(-D_RELEASE_INTERNAL)
endif("${BUILD_TYPE_lower}" STREQUAL "releaseinternal")

message(STATUS "${CMAKE_BUILD_TYPE} build configuration")

# Set the runtime source directory
if(NOT DEFINED NEO_SOURCE_DIR)
  set(NEO_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# Set the core source directory
if(NOT DEFINED NEO_CORE_DIRECTORY)
  set(NEO_CORE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/core)
endif()

if(SKIP_ALL_ULT)
    set(SKIP_UNIT_TESTS TRUE)
endif()

# Set our build directory
if(NOT DEFINED NEO_BUILD_DIR)
  set(NEO_BUILD_DIR ${CMAKE_BINARY_DIR})
endif()

if(NOT NEO_BINARY_DIR)
  set(NEO_BINARY_DIR ${CMAKE_BINARY_DIR})
endif()

# we use c++14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# we force using response files
set(CMAKE_NINJA_FORCE_RESPONSE_FILE 1)

# set output paths
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${NEO_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${NEO_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${NEO_BINARY_DIR}/lib)

# We want to build with the static, multithreaded runtime libraries (as opposed
# to the multithreaded runtime DLLs)
if(MSVC)
  string(REPLACE "/MDd" "/MTd" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
  string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASEINTERNAL "${CMAKE_CXX_FLAGS_RELEASEINTERNAL}")
endif(MSVC)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(NEO_BITS "64")
  set(NEO_ARCH "x64")
else()
  set(NEO_BITS "32")
  set(NEO_ARCH "x86")
endif()

if(NOT KMDAF_HEADERS_DIR)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../kmdaf/inc/common/kmDaf.h")
        get_filename_component(KMDAF_HEADERS_DIR "../kmdaf/inc/common/" ABSOLUTE)
        message(STATUS "KM-DAF headers dir: ${KMDAF_HEADERS_DIR}")
    endif()
endif(NOT KMDAF_HEADERS_DIR)

if(NOT DEFINED SOURCE_LEVEL_DEBUGGER_HEADERS_DIR)
    get_filename_component(SOURCE_LEVEL_DEBUGGER_HEADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/source_level_debugger/" ABSOLUTE)
    message(STATUS "Source Level Debugger headers dir: ${SOURCE_LEVEL_DEBUGGER_HEADERS_DIR}")
endif()

get_filename_component(AUB_STREAM_HEADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/aub_stream/headers" ABSOLUTE)
if(IS_DIRECTORY ${AUB_STREAM_HEADERS_DIR})
    message(STATUS "Aub Stream Headers dir: ${AUB_STREAM_HEADERS_DIR}")
else()
    message(FATAL_ERROR "Aub Stream headers not available!")
endif()

if(NOT DEFINED AUB_STREAM_DIR)
    get_filename_component(TEMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../aub_stream/aub_mem_dump" ABSOLUTE)
    if(IS_DIRECTORY ${TEMP_DIR})
        set(AUB_STREAM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../aub_stream")
        message(STATUS "Aub Stream dir: ${AUB_STREAM_DIR}")
    endif()
endif()

if(DISABLE_AUB_STREAM)
    unset(AUB_STREAM_DIR)
endif()

get_filename_component(I915_INCLUDES_DIR "${NEO_SOURCE_DIR}/third_party${BRANCH_DIR_SUFFIX}/uapi" ABSOLUTE)
message(STATUS "i915 includes dir: ${I915_INCLUDES_DIR}")

if(NOT DEFINED KHRONOS_HEADERS_DIR)
  get_filename_component(DIR_tmp "${CMAKE_CURRENT_SOURCE_DIR}/third_party/opencl_headers" ABSOLUTE)
  if(IS_DIRECTORY ${DIR_tmp})
    set(KHRONOS_HEADERS_DIR ${DIR_tmp})
    add_definitions(-DCL_TARGET_OPENCL_VERSION=220)
  else()
    message(FATAL_ERROR "Khronos OpenCL headers not available!")
  endif()
endif()
message(STATUS "Khronos OpenCL headers dir: ${KHRONOS_HEADERS_DIR}")
set(OCL_HEADERS_DIR ${KHRONOS_HEADERS_DIR})

if(NOT DEFINED KHRONOS_GL_HEADERS_DIR)
  get_filename_component(GL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/opengl_headers" ABSOLUTE)
  if(IS_DIRECTORY ${GL_DIR})
    set(KHRONOS_GL_HEADERS_DIR ${GL_DIR})
    add_definitions(-DGL_TARGET_OPENGL_VERSION=210)
  endif()
endif()
message(STATUS "Khronos OpenGL headers dir: ${KHRONOS_GL_HEADERS_DIR}")

if(NOT THIRD_PARTY_DIR)
  get_filename_component(THIRD_PARTY_DIR "../third_party/" ABSOLUTE)
endif(NOT THIRD_PARTY_DIR)
message(STATUS "Third party dir: ${THIRD_PARTY_DIR}")

if(WIN32)
# get WDK location and version to use
  if(NOT WDK_DIR)
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../wdk")
      get_filename_component(WDK_DIR "../wdk" ABSOLUTE)
    endif()
  endif(NOT WDK_DIR)
  if(WDK_DIR)
    if(IS_DIRECTORY "${WDK_DIR}/Win15")
      get_filename_component(WDK_DIR "${WDK_DIR}/Win15" ABSOLUTE)
    endif()
  endif()
  message(STATUS "WDK Directory: ${WDK_DIR}")

  if(NOT WDK_VERSION)
    # Get WDK version from ${WDK_DIR}/WDKVersion.txt
    file(READ "${WDK_DIR}/WDKVersion.txt" WindowsTargetPlatformVersion)
    string(REPLACE " " ";" WindowsTargetPlatformVersion ${WindowsTargetPlatformVersion})
    list(LENGTH WindowsTargetPlatformVersion versionListLength)
    if(NOT versionListLength EQUAL 3)
      message(ERROR "Error reading content of WDKVersion.txt file")
    endif(NOT versionListLength EQUAL 3)
    list(GET WindowsTargetPlatformVersion 2 WindowsTargetPlatformVersion)
  else()
    set(WindowsTargetPlatformVersion ${WDK_VERSION})
  endif()
  message(STATUS "WDK Version is ${WindowsTargetPlatformVersion}")

  if(${WindowsTargetPlatformVersion} VERSION_LESS "10.0.18328.0")
    set(CONST_FROM_WDK_10_0_18328_0)
  else()
    set(CONST_FROM_WDK_10_0_18328_0 "CONST")
  endif()
  add_compile_options(-DCONST_FROM_WDK_10_0_18328_0=${CONST_FROM_WDK_10_0_18328_0})

  set(WDK_INCLUDE_PATHS
    "${WDK_DIR}/Include/${WindowsTargetPlatformVersion}/um"
    "${WDK_DIR}/Include/${WindowsTargetPlatformVersion}/shared"
    "${WDK_DIR}/Include/${WindowsTargetPlatformVersion}/km"
  )
  message(STATUS "WDK include paths: ${WDK_INCLUDE_PATHS}")
endif()

# Intel Graphics Compiler detection
if(NOT IGC__IGC_TARGETS)
    # check whether igc is part of workspace
    if(DEFINED IGC_DIR)
        get_filename_component(IGC_DIR "${IGC_DIR}" ABSOLUTE)
    else()
        get_filename_component(IGC_DIR_tmp "${NEO_SOURCE_DIR}/../igc" ABSOLUTE)
        if(IS_DIRECTORY "${IGC_DIR_tmp}")
            set(IGC_DIR "${IGC_DIR_tmp}")
        endif()
    endif()

    if(UNIX)
        # on Unix-like use pkg-config
        find_package(PkgConfig)
        if(DEFINED IGC_DIR)
            if(IS_DIRECTORY "${IGC_DIR}/lib/pkgconfig/")
                set(__tmp_LIBDIR "lib")
            elseif(IS_DIRECTORY "${IGC_DIR}/${CMAKE_INSTALL_LIBDIR}/pkgconfig/")
                set(__tmp_LIBDIR ${CMAKE_INSTALL_LIBDIR})
            endif()
        endif()
        if(DEFINED __tmp_LIBDIR)
            set(OLD_PKG_CONFIG_PATH $ENV{PKG_CONFIG_PATH})
            set(ENV{PKG_CONFIG_PATH} "${IGC_DIR}/${__tmp_LIBDIR}/pkgconfig")
        endif()
        pkg_check_modules(IGDRCL__IGC igc-opencl)
        if(DEFINED __tmp_LIBDIR)
            set(ENV{PKG_CONFIG_PATH} ${OLD_PKG_CONFIG_PATH})
            set(IGDRCL__IGC_LIBRARY_PATH "${IGC_DIR}/${__tmp_LIBDIR}/")
        endif()

        if(IGDRCL__IGC_FOUND)
            if(DEFINED IGC_DIR AND IS_DIRECTORY "${IGC_DIR}/${__tmp_LIBDIR}/pkgconfig/")
                string(REPLACE "${IGDRCL__IGC_INCLUDEDIR}" "${IGC_DIR}/include/igc" IGDRCL__IGC_INCLUDE_DIRS "${IGDRCL__IGC_INCLUDE_DIRS}")
            endif()

            set(IGDRCL__IGC_INCLUDE_DIR ${IGDRCL__IGC_INCLUDE_DIRS})
            message(STATUS "IGC include dirs: ${IGDRCL__IGC_INCLUDE_DIR}")
        endif()
        if(DEFINED __tmp_LIBDIR)
            unset(__tmp_LIBDIR)
        endif()
    endif()

    if(IGDRCL__IGC_FOUND)
        # do nothing
    elseif(EXISTS "${IGC_DIR}/CMakeLists.txt")
        message(STATUS "IGC source dir is: ${IGC_DIR}")

        set(IGC_OPTION__OUTPUT_DIR "${NEO_BUILD_DIR}/igc")
        set(IGC_OPTION__INCLUDE_IGC_COMPILER_TOOLS OFF)
        add_subdirectory_unique("${IGC_DIR}" "${NEO_BUILD_DIR}/igc" EXCLUDE_FROM_ALL)

        set(IGDRCL__IGC_TARGETS "${IGC__IGC_TARGETS}")
        foreach(TARGET_tmp ${IGDRCL__IGC_TARGETS})
            list(APPEND IGDRCL__IGC_INCLUDE_DIR $<TARGET_PROPERTY:${TARGET_tmp},INTERFACE_INCLUDE_DIRECTORIES>)
            list(APPEND IGDRCL__IGC_COMPILE_DEFINITIONS $<TARGET_PROPERTY:${TARGET_tmp},INTERFACE_COMPILE_DEFINITIONS>)
        endforeach(TARGET_tmp)
        message(STATUS "IGC targets: ${IGDRCL__IGC_TARGETS}")
    else()
        message(FATAL_ERROR "Intel Graphics Compiler not found!")
    endif()
else()
    set(IGDRCL__IGC_TARGETS "${IGC__IGC_TARGETS}")
    foreach(TARGET_tmp ${IGDRCL__IGC_TARGETS})
        list(APPEND IGDRCL__IGC_INCLUDE_DIR $<TARGET_PROPERTY:${TARGET_tmp},INTERFACE_INCLUDE_DIRECTORIES>)
        list(APPEND IGDRCL__IGC_LIBRARY_PATH $<TARGET_FILE_DIR:${TARGET_tmp}>)
    endforeach(TARGET_tmp)
    string(REPLACE ";" ":" IGDRCL__IGC_LIBRARY_PATH "${IGDRCL__IGC_LIBRARY_PATH}")
    message(STATUS "IGC targets: ${IGDRCL__IGC_TARGETS}")
endif()

# VISA headers - always relative to IGC
if(IS_DIRECTORY "${IGC_DIR}/../visa")
    get_filename_component(VISA_DIR "${IGC_DIR}/../visa" ABSOLUTE)
elseif(IS_DIRECTORY "${IGC_DIR}/visa")
    set(VISA_DIR "${IGC_DIR}/visa")
elseif(IS_DIRECTORY "${IGC_DIR}/include/visa")
    set(VISA_DIR "${IGC_DIR}/include/visa")
elseif(IS_DIRECTORY "${IGDRCL__IGC_INCLUDEDIR}/../visa")
    get_filename_component(VISA_DIR "${IGDRCL__IGC_INCLUDEDIR}/../visa" ABSOLUTE)
elseif(IS_DIRECTORY "${IGC_OCL_ADAPTOR_DIR}/../../visa")
    get_filename_component(VISA_DIR "${IGC_OCL_ADAPTOR_DIR}/../../visa" ABSOLUTE)
endif()
message(STATUS "VISA Dir: ${VISA_DIR}")

if(IS_DIRECTORY "${VISA_DIR}/include")
    set(VISA_INCLUDE_DIR "${VISA_DIR}/include")
else()
    set(VISA_INCLUDE_DIR "${VISA_DIR}")
endif()

# IGA headers - always relative to VISA
if(IS_DIRECTORY "${VISA_DIR}/../iga")
    get_filename_component(IGA_DIR "${VISA_DIR}/../iga" ABSOLUTE)
elseif(IS_DIRECTORY "${VISA_DIR}/iga")
    set(IGA_DIR "${VISA_DIR}/iga")
endif()

if(IS_DIRECTORY "${IGA_DIR}/IGALibrary/api")
    set(IGA_INCLUDE_DIR "${IGA_DIR}/IGALibrary/api")
else()
    set(IGA_INCLUDE_DIR "${IGA_DIR}")
endif()

if(IS_DIRECTORY ${IGA_INCLUDE_DIR})
    set(IGA_HEADERS_AVAILABLE TRUE)
    set(IGA_LIBRARY_NAME "iga${NEO_BITS}")
else()
    set(IGA_HEADERS_AVAILABLE FALSE)
endif()

message(STATUS "IGA Includes dir: ${IGA_INCLUDE_DIR}")

if(WIN32)
    set(IGC_LIBRARY_NAME "igc${NEO_BITS}")
    set(FCL_LIBRARY_NAME "igdfcl${NEO_BITS}")
endif()

# GmmLib detection
if(NOT DEFINED GMMUMD_LIB_NAME)
  if(TARGET igfx_gmmumd_dll)
    set(GMMUMD_LIB_NAME "igfx_gmmumd_dll")
    if(UNIX)
      set(IGDRCL__GMM_LIBRARY_PATH "$<TARGET_FILE_DIR:${GMMUMD_LIB_NAME}>")
    endif()
  else()
    if(DEFINED GMM_DIR)
      get_filename_component(GMM_DIR "${GMM_DIR}" ABSOLUTE)
    else()
      get_filename_component(GMM_DIR_tmp "${NEO_SOURCE_DIR}/../gmmlib" ABSOLUTE)
      if(IS_DIRECTORY "${GMM_DIR_tmp}")
        set(GMM_DIR "${GMM_DIR_tmp}")
      endif()
    endif()

    if(UNIX)
      if(DEFINED GMM_DIR)
        if(IS_DIRECTORY "${GMM_DIR}/lib/pkgconfig/")
          set(__tmp_LIBDIR "lib")
        elseif(IS_DIRECTORY "${GMM_DIR}/${CMAKE_INSTALL_LIBDIR}/pkgconfig/")
          set(__tmp_LIBDIR ${CMAKE_INSTALL_LIBDIR})
        endif()
      endif()

      find_package(PkgConfig)
      if(DEFINED __tmp_LIBDIR)
        set(OLD_PKG_CONFIG_PATH $ENV{PKG_CONFIG_PATH})
        set(ENV{PKG_CONFIG_PATH} "${GMM_DIR}/${__tmp_LIBDIR}/pkgconfig/")
      endif()
      pkg_check_modules(IGDRCL__GMM igdgmm)
      if(DEFINED __tmp_LIBDIR)
        set(ENV{PKG_CONFIG_PATH} ${OLD_PKG_CONFIG_PATH})
      endif()

      if(IGDRCL__GMM_FOUND)
        if(DEFINED __tmp_LIBDIR)
          string(REPLACE "${IGDRCL__GMM_INCLUDEDIR}" "${GMM_DIR}/include/igdgmm" IGDRCL__GMM_INCLUDE_DIRS "${IGDRCL__GMM_INCLUDE_DIRS}")
          string(REPLACE "${IGDRCL__GMM_LIBDIR}" "${GMM_DIR}/${__tmp_LIBDIR}" IGDRCL__GMM_LIBDIR "${IGDRCL__GMM_LIBDIR}")
          set(IGDRCL__GMM_LIBRARY_PATH "${IGDRCL__GMM_LIBDIR}")
        endif()

        set(GMMUMD_LIB_NAME "igfx_gmmumd_dll")

        set(IGDRCL__GMM_INCLUDE_DIR ${IGDRCL__GMM_INCLUDE_DIRS})
        set(UMKM_SHAREDDATA_INCLUDE_PATHS "${IGDRCL__GMM_INCLUDE_DIR}")
        message(STATUS "GmmLib include dirs: ${IGDRCL__GMM_INCLUDE_DIR}")
      else()
        message(FATAL_ERROR "GmmLib not found!")
      endif()
      if(DEFINED __tmp_LIBDIR)
          unset(__tmp_LIBDIR)
      endif()
    else()
      if(EXISTS "${GMM_DIR}/CMakeLists.txt")
        message(STATUS "GmmLib source dir is: ${GMM_DIR}")
        add_subdirectory_unique("${GMM_DIR}" "${NEO_BUILD_DIR}/gmmlib")

        if(NOT DEFINED GMMUMD_LIB_NAME)
          set(GMMUMD_LIB_NAME "igfx_gmmumd_dll")
        endif()

        set(UMKM_SHAREDDATA_INCLUDE_PATHS $<TARGET_PROPERTY:${GMMUMD_LIB_NAME},INTERFACE_INCLUDE_DIRECTORIES>)
      else()
        message(FATAL_ERROR "GmmLib not found!")
      endif()
    endif()
  endif()
else()
  set(UMKM_SHAREDDATA_INCLUDE_PATHS $<TARGET_PROPERTY:${GMMUMD_LIB_NAME},INTERFACE_INCLUDE_DIRECTORIES>)
  if(UNIX)
    set(IGDRCL__GMM_LIBRARY_PATH "$<TARGET_FILE_DIR:${GMMUMD_LIB_NAME}>")
  endif()
endif()

macro(copy_gmm_dll_for target)
  if(NOT UNIX)
    add_dependencies(${target} ${GMMUMD_LIB_NAME})
    add_custom_command(
      TARGET ${target}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${GMMUMD_LIB_NAME}> $<TARGET_FILE_DIR:${target}>
    )
  endif()
endmacro()

# Instrumentation detection
if(NOT DEFINED INSTRUMENTATION_LIB_NAME)
  if(DEFINED INSTRUMENTATION_SOURCE_DIR)
    get_filename_component(INSTRUMENTATION_SOURCE_DIR "${INSTRUMENTATION_SOURCE_DIR}" ABSOLUTE)
  else()
    get_filename_component(INSTRUMENTATION_SOURCE_DIR_tmp "${NEO_SOURCE_DIR}/../instrumentation" ABSOLUTE)
    if(IS_DIRECTORY "${INSTRUMENTATION_SOURCE_DIR_tmp}")
      set(INSTRUMENTATION_SOURCE_DIR "${INSTRUMENTATION_SOURCE_DIR_tmp}")
    endif()
  endif()
  if(IS_DIRECTORY "${INSTRUMENTATION_SOURCE_DIR}")
    message(STATUS "Instrumentation source dir is: ${INSTRUMENTATION_SOURCE_DIR}")
    set(INSTRUMENTATION_INCLUDE_PATHS ${INSTRUMENTATION_SOURCE_DIR}/inc)
    set(HAVE_INSTRUMENTATION TRUE)
  else()
    message(STATUS "Instrumentation source dir not found")
    set(HAVE_INSTRUMENTATION FALSE)
  endif()
else()
  if(TARGET ${INSTRUMENTATION_LIB_NAME})
    set(INSTRUMENTATION_INCLUDE_PATHS $<TARGET_PROPERTY:${INSTRUMENTATION_LIB_NAME},INTERFACE_INCLUDE_DIRECTORIES>)
    set(HAVE_INSTRUMENTATION TRUE)
  else()
    message(STATUS "Instrumentation target not found. ${INSTRUMENTATION_LIB_NAME} is not a valid target")
    set(HAVE_INSTRUMENTATION FALSE)
  endif()
endif()

if(NOT DISABLE_LIBVA)
    # LibVA detection
    if(UNIX)
      set(IGDRCL__LIBVA_IS_REQUIRED "")
      if(IGDRCL_FORCE_USE_LIBVA)
        set(IGDRCL__LIBVA_IS_REQUIRED "REQUIRED")
      endif()

      if(DEFINED LIBVA_SOURCE_DIR)
        get_filename_component(LIBVA_SOURCE_DIR "${LIBVA_SOURCE_DIR}" ABSOLUTE)
      else()
        get_filename_component(LIBVA_SOURCE_DIR_tmp "${NEO_SOURCE_DIR}/../libva" ABSOLUTE)
        if(IS_DIRECTORY "${LIBVA_SOURCE_DIR_tmp}")
          set(LIBVA_SOURCE_DIR "${LIBVA_SOURCE_DIR_tmp}")
        endif()
      endif()

      find_package(PkgConfig)
      if(DEFINED LIBVA_SOURCE_DIR AND IS_DIRECTORY "${LIBVA_SOURCE_DIR}/lib/pkgconfig/")
        set(OLD_PKG_CONFIG_PATH $ENV{PKG_CONFIG_PATH})
        set(ENV{PKG_CONFIG_PATH} "${LIBVA_SOURCE_DIR}/lib/pkgconfig/")
      endif()
      pkg_check_modules(IGDRCL__LIBVA ${IGDRCL__LIBVA_IS_REQUIRED} libva>=1.0.0)
      include(CheckLibraryExists)
      if(DEFINED LIBVA_SOURCE_DIR AND IS_DIRECTORY "${LIBVA_SOURCE_DIR}/lib/pkgconfig/")
        set(ENV{PKG_CONFIG_PATH} ${OLD_PKG_CONFIG_PATH})
      endif()

      if(IGDRCL__LIBVA_FOUND)
        CHECK_LIBRARY_EXISTS(va vaGetLibFunc ${IGDRCL__LIBVA_LIBDIR} HAVE_VAGETLIBFUNC)

        add_definitions(-DLIBVA)
        message(STATUS "Using libva ")

        if(DEFINED LIBVA_SOURCE_DIR AND IS_DIRECTORY "${LIBVA_SOURCE_DIR}/lib/pkgconfig/")
          string(REPLACE "${IGDRCL__LIBVA_INCLUDEDIR}" "${LIBVA_SOURCE_DIR}/include" IGDRCL__LIBVA_INCLUDE_DIRS "${IGDRCL__LIBVA_INCLUDE_DIRS}")
          string(REPLACE "${IGDRCL__LIBVA_LIBDIR}" "${LIBVA_SOURCE_DIR}/lib" IGDRCL__LIBVA_LIBDIR "${IGDRCL__LIBVA_LIBDIR}")
          set(IGDRCL__LIBVA_LIBRARY_PATH "${IGDRCL__LIBVA_LIBDIR}")
        endif()

        list(LENGTH IGDRCL__LIBVA_INCLUDE_DIRS __len)
        if(__len GREATER 0)
          set(IGDRCL__LIBVA_INCLUDE_DIR ${IGDRCL__LIBVA_INCLUDE_DIRS})
          include_directories("${IGDRCL__LIBVA_INCLUDE_DIR}")
          message(STATUS "LibVA include dirs: ${IGDRCL__LIBVA_INCLUDE_DIR}")
        endif()
      endif()
    endif()
endif()

set(SLD_LIBRARY_NAME "igfxdbgxchg")
if(NEO_BITS STREQUAL "64")
  set(SLD_LIBRARY_NAME "${SLD_LIBRARY_NAME}${NEO_BITS}")
endif()


add_subdirectory_unique(third_party/gtest)

message(STATUS "AUB_STREAM_DIR = ${AUB_STREAM_DIR}" )
if(DEFINED AUB_STREAM_DIR)
    set(AUB_STREAM_PROJECT_NAME "aub_stream")
    add_subdirectory_unique(${AUB_STREAM_DIR} ${CMAKE_BINARY_DIR}/aub_stream EXCLUDE_FROM_ALL)
    set(ENGINE_NODE_DIR ${AUB_STREAM_DIR}/aub_mem_dump${BRANCH_DIR_SUFFIX}/includes)
else()
    set(ENGINE_NODE_DIR ${AUB_STREAM_HEADERS_DIR})
endif()
message(STATUS "Engine node dir: ${ENGINE_NODE_DIR}")

add_definitions(-DGMM_OCL)

# We want to organize our IDE targets into folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# determine Neo version
include(version.cmake)

# Get available platfroms
include(platforms.cmake)

# Enable/Disable BuiltIns compilation during build
set(COMPILE_BUILT_INS TRUE CACHE BOOL "Enable built-in kernels compilation")

# Changing the default executable and library output directories
set(IGDRCL_OUTPUT_DIR "${IGDRCL_OPTION__OUTPUT_DIR}")

# do not add rpath
set(CMAKE_SKIP_RPATH YES CACHE BOOL "" FORCE )

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

option(APPVERIFIER_ALLOWED "allow use of AppVerifier" OFF)

option(CCACHE_ALLOWED "allow use of ccache" TRUE)
find_program(CCACHE_EXE_FOUND ccache)
if(CCACHE_EXE_FOUND AND CCACHE_ALLOWED)
  message(STATUS "Found ccache: ${CCACHE_EXE_FOUND}")
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif()

# Miscs options
option(IGDRCL_GCOV "generate gcov report" OFF)
option(USE_CL_CACHE "Use OpenCL program binary cache" ON)
set(CL_CACHE_LOCATION "cl_cache" CACHE STRING "OpenCL program binary cache location")

# Put profiling enable flag into define
if(OCL_RUNTIME_PROFILING)
  add_definitions(-DOCL_RUNTIME_PROFILING=${OCL_RUNTIME_PROFILING})
endif()

if(MSVC)
  # Force to treat warnings as errors
  if(NOT CMAKE_CXX_FLAGS MATCHES "/WX")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX")
        if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /we4189")
        endif()
  endif()

# Disable generating manifest
  set(LINKER_FLAGS "/MANIFEST:NO")

# Support for WUD
  set(CMAKE_CXX_STANDARD_LIBRARIES "onecore.lib")
  foreach(IT kernel32.lib;user32.lib;gdi32.lib;advapi32.lib;ole32.lib;)
    set(LINKER_FLAGS "${LINKER_FLAGS} /NODEFAULTLIB:${IT}")
  endforeach()
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LINKER_FLAGS}")
  set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${LINKER_FLAGS}")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${LINKER_FLAGS}")
  add_definitions(-DUNICODE -D_UNICODE)
  link_directories("${WDK_DIR}/Lib/${WindowsTargetPlatformVersion}/um/${NEO_ARCH}/")
else()
  if(IGDRCL_GCOV)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage --coverage")
  endif()
  option(USE_ASAN "Link with address sanitization support" OFF)
  if(USE_ASAN)
    if(CMAKE_COMPILER_IS_GNUCC)
      set(ASAN_FLAGS " -fsanitize=address -fno-omit-frame-pointer -DSANITIZER_BUILD")
      set(ASAN_LIBS "asan")
    else()
      message(STATUS "Address sanitization with clang not yet support")
    endif()
  endif()
  if(USE_TSAN)
    if(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
      set(TSAN_FLAGS " -fsanitize=thread -DSANITIZER_BUILD")
      set(TSAN_LIBS "tsan")
    else()
      message(STATUS "Thread sanitization with gcc is not fully supported")
    endif()
  endif()

endif()

# setup variables needed for custom configuration type
# generate PDB files even for release build on MSVC
if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
  set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")

  set(CMAKE_CXX_FLAGS_RELEASEINTERNAL "${CMAKE_CXX_FLAGS_RELEASEINTERNAL} /Zi")
  set(CMAKE_SHARED_LINKER_FLAGS_RELEASEINTERNAL "${CMAKE_SHARED_LINKER_FLAGS_RELEASEINTERNAL} /DEBUG /OPT:REF /OPT:ICF")

  if(NO_PDB)
    string(REGEX REPLACE "/Zi" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    string(REGEX REPLACE "/Zi" "" CMAKE_CXX_FLAGS_RELEASEINTERNAL "${CMAKE_CXX_FLAGS_RELEASEINTERNAL}")
    string(REGEX REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  endif()
endif()

# spectre mitigation
include(CheckCXXCompilerFlag)
if(MSVC)
  check_cxx_compiler_flag(/Qspectre COMPILER_SUPPORTS_QSPECTRE)
  check_cxx_compiler_flag(/d2guardspecload COMPILER_SUPPORTS_D2GUARDSPECLOAD)
  if(COMPILER_SUPPORTS_QSPECTRE)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Qspectre")
  elseif(COMPILER_SUPPORTS_D2GUARDSPECLOAD)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /d2guardspecload")
  else()
    message(WARNING "Spectre mitigation is not supported by the compiler")
  endif()
else()
  if(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    check_cxx_compiler_flag(-mretpoline COMPILER_SUPPORTS_RETPOLINE)
    if(COMPILER_SUPPORTS_RETPOLINE)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mretpoline")
    else()
      message(WARNING "Spectre mitigation -mretpoline flag is not supported by the compiler")
    endif()
  else()
    check_cxx_compiler_flag(-mindirect-branch=thunk COMPILER_SUPPORTS_INDIRECT_BRANCH_THUNK)
    if(COMPILER_SUPPORTS_INDIRECT_BRANCH_THUNK)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mindirect-branch=thunk")
    else()
      message(WARNING "Spectre mitigation -mindirect-branch=thunk flag is not supported by the compiler")
    endif()
    check_cxx_compiler_flag(-mfunction-return=thunk COMPILER_SUPPORTS_FUNCTION_RETURN_THUNK)
    if(COMPILER_SUPPORTS_FUNCTION_RETURN_THUNK)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfunction-return=thunk")
    else()
      message(WARNING "Spectre mitigation -mfunction-return=thunk flag is not supported by the compiler")
    endif()
    check_cxx_compiler_flag(-mindirect-branch-register COMPILER_SUPPORTS_INDIRECT_BRANCH_REGISTER)
    if(COMPILER_SUPPORTS_INDIRECT_BRANCH_REGISTER)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mindirect-branch-register")
    else()
      message(WARNING "Spectre mitigation -mindirect-branch-register flag is not supported by the compiler")
    endif()
  endif()
endif(MSVC)

if(NOT MSVC)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-depth=1024")
endif(NOT MSVC)

# Compiler warning flags
if(NOT MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wempty-body")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wignored-qualifiers")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wtype-limits")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wuninitialized")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra -Wno-unused-parameter -Wno-missing-field-initializers")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Winvalid-pch")

  if("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang" )
    # clang only
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wshorten-64-to-32")
    if(USE_SANITIZE_UB)
      message(STATUS "Enabling undefined behavior sanitizer")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fsanitize-recover=vptr -fno-rtti -DSANITIZER_BUILD")
      if(NOT SANITIZE_UB_ALLOW_CONTINUE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-sanitize-recover=undefined")
      endif()
    endif(USE_SANITIZE_UB)
    if (NOT (CMAKE_C_COMPILER_VERSION VERSION_LESS 3.6))
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-local-typedefs -DSANITIZER_BUILD")
    endif()
    if (NOT (CMAKE_C_COMPILER_VERSION VERSION_LESS 4.0))
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-register") # Added for htons()
    endif()
  else()
    # gcc only
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-local-typedefs -Wno-unused-but-set-variable")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wclobbered")
    if (CMAKE_C_COMPILER_VERSION VERSION_LESS 7.0)
    else()
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wimplicit-fallthrough=4")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-noexcept-type")  # Added for gtest
    endif()
  endif()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
endif()

# Compile code with defenses enabled (settings to be used for production release code)
if("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
  if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GS")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /sdl")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /NXCompat")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DynamicBase")
    if("${NEO_ARCH}" STREQUAL "x86")
      set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /SafeSEH")
    endif()
  else()
    if(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-strong")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -D_FORTIFY_SOURCE=2")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wformat -Wformat-security")
    else()
      # gcc, g++ only
      if (CMAKE_C_COMPILER_VERSION VERSION_LESS 4.9)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector")
      else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-strong")
      endif()
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -D_FORTIFY_SOURCE=2")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wformat -Wformat-security")
      set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -Wl,-z,noexecstack")
      set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -Wl,-z,relro")
      set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -Wl,-z,now")
    endif()
  endif()
endif()

# Project-wide include paths
# Please keep alphabetical order
include_directories(${NEO_BUILD_DIR})
include_directories(${NEO_SOURCE_DIR})
include_directories(${NEO_SOURCE_DIR}/core/memory_manager/definitions${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SOURCE_DIR}/core/memory_properties${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SOURCE_DIR}/core/sku_info/definitions${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SOURCE_DIR}/runtime/built_ins/builtinops${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SOURCE_DIR}/runtime/command_stream/definitions${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SOURCE_DIR}/runtime/gen_common/reg_configs${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SOURCE_DIR}/runtime/gmm_helper/${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SOURCE_DIR}/runtime/gmm_helper/client_context${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SOURCE_DIR}/runtime/gmm_helper/gmm_memory${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SOURCE_DIR}/runtime/mem_obj/definitions${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SOURCE_DIR}/runtime/memory_manager/definitions${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SOURCE_DIR}/runtime/os_interface/definitions${BRANCH_DIR_SUFFIX})

if(HAVE_INSTRUMENTATION)
  set(IGDRCL__INSTRUMENTATION_DIR_SUFFIX ${BRANCH_DIR_SUFFIX})
  message(STATUS "Instrumentation include dirs: ${INSTRUMENTATION_INCLUDE_PATHS}")
  include_directories(${INSTRUMENTATION_INCLUDE_PATHS})
else()
  set(IGDRCL__INSTRUMENTATION_DIR_SUFFIX "/")
endif()
include_directories(${NEO_SOURCE_DIR}/runtime/instrumentation${IGDRCL__INSTRUMENTATION_DIR_SUFFIX})

# Define where to put binaries
if(MSVC)
	if ("${CMAKE_GENERATOR}" STREQUAL "Ninja")
		set(TargetDir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
	else()
		set(TargetDir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE})
	endif()
else()
	set(TargetDir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif(MSVC)

add_subdirectory_unique(offline_compiler ${NEO_BUILD_DIR}/offline_compiler)
target_compile_definitions(ocloc PRIVATE MOCKABLE_VIRTUAL=)
add_subdirectory_unique(core)

macro(generate_runtime_lib LIB_NAME MOCKABLE GENERATE_EXEC)
	set(NEO_STATIC_LIB_NAME ${LIB_NAME})
	set(SHARINGS_ENABLE_LIB_NAME "${LIB_NAME}_sharings_enable")
	set(GENERATE_EXECUTABLE ${GENERATE_EXEC})
	if(NOT BUILD_WITHOUT_RUNTIME)
		add_subdirectory(runtime "${NEO_BUILD_DIR}/${LIB_NAME}")
	else()
		add_subdirectory(runtime "${NEO_BUILD_DIR}/${LIB_NAME}" EXCLUDE_FROM_ALL)
	endif()
	target_compile_definitions(${BUILTINS_SOURCES_LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=)
	target_compile_definitions(${BUILTINS_BINARIES_LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=)
	target_compile_definitions(${SCHEDULER_BINARY_LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=)


	if(${MOCKABLE})
		target_compile_definitions(${LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=virtual)
		target_compile_definitions(${SHARINGS_ENABLE_LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=virtual)
		target_compile_definitions(${LIB_NAME} PUBLIC DEFAULT_TEST_PLATFORM=${DEFAULT_TESTED_PLATFORM} DEFAULT_TEST_FAMILY_NAME=${DEFAULT_TESTED_FAMILY_NAME} ${TESTED_GEN_FLAGS_DEFINITONS})
	else()
		target_compile_definitions(${LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=)
		target_compile_definitions(${SHARINGS_ENABLE_LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=)
	endif()
endmacro(generate_runtime_lib)

set(NEO_DYNAMIC_LIB_NAME "igdrcl_dll") # single NEO dll
set(NEO_DLL_NAME_BASE "igdrcl")

set(BIKSIM_LIB_NAME "biksim")
set(BUILTINS_SOURCES_LIB_NAME "builtins_sources")
set(BUILTINS_BINARIES_LIB_NAME "builtins_binaries")
set(SCHEDULER_BINARY_LIB_NAME "scheduler_binary")

if(DONT_CARE_OF_VIRTUALS)
    message(STATUS "All targets will use virtuals")

    set(NEO_RELEASE_LIB_NAME "igdrcl_lib")
    set(NEO_MOCKABLE_LIB_NAME ${NEO_RELEASE_LIB_NAME})
    generate_runtime_lib(${NEO_RELEASE_LIB_NAME} TRUE TRUE)
else()
    set(NEO_RELEASE_LIB_NAME "igdrcl_lib_release") # Used by dll/so
    generate_runtime_lib(${NEO_RELEASE_LIB_NAME} FALSE TRUE)
    if(NOT SKIP_ALL_ULT)
        set(NEO_MOCKABLE_LIB_NAME "igdrcl_lib_mockable") # Used by ULTS
        generate_runtime_lib(${NEO_MOCKABLE_LIB_NAME} TRUE FALSE)
    endif()
endif()

if(DEFAULT_TESTED_PLATFORM)
  add_subdirectory_unique(unit_tests ${NEO_BUILD_DIR}/unit_tests)
else()
  hide_subdir(unit_tests)
endif()

set(DONT_LINK_ELF_STATICALLY TRUE)
if(EXISTS ${NEO_SOURCE_DIR}/../internal)
  add_subdirectory_unique(${NEO_SOURCE_DIR}/../internal ${NEO_BUILD_DIR}/internal)
endif(EXISTS ${NEO_SOURCE_DIR}/../internal)

add_subdirectory_unique(scripts/lint)

configure_file(config.h.in ${NEO_BUILD_DIR}/config.h)
if(WIN32 AND NOT IGDRCL__IGC_FOUND)
    configure_file(igc.opencl.h.in ${NEO_BUILD_DIR}/igc.opencl.h)
endif()
configure_file(driver_version.h.in ${NEO_BUILD_DIR}/driver_version.h) # Put Driver version into define
configure_file(lib_names.h.in ${NEO_BUILD_DIR}/lib_names.h)

hide_subdir(runtime)
add_subdirectories()

if(NEO_BUILD_PACKAGE)
    include(package.cmake)
endif()
