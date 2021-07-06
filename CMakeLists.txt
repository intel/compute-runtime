#
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  cmake_minimum_required(VERSION 3.13.0 FATAL_ERROR)
else()
  cmake_minimum_required(VERSION 3.2.0 FATAL_ERROR)
endif()

include(ExternalProject)

# Set the runtime source directory
if(NOT DEFINED NEO_SOURCE_DIR)
  set(NEO_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif()

if(MSVC)
  set(CMAKE_GENERATOR_TOOLSET "host=x64")
endif()

if(NOT PROJECT_NAME)
  set(PROJECT_NAME igdrcl)
endif()

project(${PROJECT_NAME})

file(READ ".branch" BRANCH_TYPE)
string(STRIP "${BRANCH_TYPE}" BRANCH_TYPE)
set(BRANCH_DIR_SUFFIX "/${BRANCH_TYPE}")
list(APPEND BRANCH_DIR_LIST "/")
if(NOT "${BRANCH_TYPE}" STREQUAL "")
  list(APPEND BRANCH_DIR_LIST "/${BRANCH_TYPE}/")
endif()
message(STATUS "branch dir list: ${BRANCH_DIR_LIST}")

# Include custom configs
include("${BRANCH_TYPE}/custom_config.cmake" OPTIONAL)

# Include needed macros
include(common_macros.cmake)

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
define_property(GLOBAL PROPERTY NEO_OCL_COMPONENTS_LIST BRIEF_DOCS "List of components" FULL_DOCS "List of components to create packages")
set(GLOBAL PROPERTY NEO_OCL_COMPONENTS_LIST "")
define_property(GLOBAL PROPERTY NEO_L0_COMPONENTS_LIST BRIEF_DOCS "List of components" FULL_DOCS "List of components to create packages")
set(GLOBAL PROPERTY NEO_L0_COMPONENTS_LIST "")

if(WIN32)
  set(DISABLE_WDDM_LINUX TRUE)
endif()

if(WIN32)
  # get WDK location and version to use
  if(NOT WDK_DIR)
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../wdk")
      get_filename_component(WDK_DIR "../wdk" ABSOLUTE)
    endif()
  endif()
  if(WDK_DIR)
    if(IS_DIRECTORY "${WDK_DIR}/Win15")
      get_filename_component(WDK_DIR "${WDK_DIR}/Win15" ABSOLUTE)
    endif()
  endif()
  message(STATUS "WDK Directory: ${WDK_DIR}")

  if(NOT WDK_VERSION)
    if(WDK_DIR)
      # Get WDK version from ${WDK_DIR}/WDKVersion.txt
      file(READ "${WDK_DIR}/WDKVersion.txt" WindowsTargetPlatformVersion)
      string(REPLACE " " ";" WindowsTargetPlatformVersion ${WindowsTargetPlatformVersion})
      list(LENGTH WindowsTargetPlatformVersion versionListLength)
      if(NOT versionListLength EQUAL 3)
        if(WIN32)
          message(ERROR "Error reading content of WDKVersion.txt file")
        endif()
      else()
        list(GET WindowsTargetPlatformVersion 2 WindowsTargetPlatformVersion)
      endif()
    else()
      if(WIN32)
        message(ERROR "WDK not available")
      endif()
    endif()
  else()
    set(WindowsTargetPlatformVersion ${WDK_VERSION})
  endif()
  message(STATUS "WDK Version is ${WindowsTargetPlatformVersion}")
endif()

if(NOT DISABLE_WDDM_LINUX)
  get_filename_component(LIBDXG_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libdxg" ABSOLUTE)
  set(D3DKMTHK_INCLUDE_PATHS "${LIBDXG_PATH}/include/")
endif()

if(WIN32)
  set(DRIVER_MODEL wddm)
else()
  if(DISABLE_WDDM_LINUX)
    set(DRIVER_MODEL drm)
  else()
    set(DRIVER_MODEL drm_or_wddm)
  endif()
endif()
message(STATUS "Driver model : ${DRIVER_MODEL}")

if(TR_DEPRECATED)
  add_definitions(-D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING=1)
endif()

if(RELEASE_WITH_REGKEYS)
  message(STATUS "Enabling RegKey reading in release build")
  add_definitions(-D_RELEASE_BUILD_WITH_REGKEYS)
endif()

if(DEFINED NEO_EXTRA_DEFINES)
  string(REPLACE "," ";" NEO_EXTRA_DEFINES "${NEO_EXTRA_DEFINES}")
  foreach(_tmp ${NEO_EXTRA_DEFINES})
    message(STATUS "adding extra define: ${_tmp}")
    add_definitions(-D${_tmp})
  endforeach()
endif()

include("${BRANCH_TYPE}/${BRANCH_TYPE}.cmake" OPTIONAL)

if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "CMAKE_BUILD_TYPE not specified, using Release")
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type: [Release, ReleaseInternal, Debug]" FORCE)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(NEO_BITS "64")
  set(NEO_ARCH "x64")
else()
  set(NEO_BITS "32")
  set(NEO_ARCH "x86")
endif()

if(NOT DEFINED BUILD_WITH_L0)
  if("${NEO_BITS}" STREQUAL "64")
    set(BUILD_WITH_L0 TRUE)
  endif()
endif()

if(NOT "${NEO_BITS}" STREQUAL "64")
  set(BUILD_WITH_L0 FALSE)
endif()

add_definitions(
                -DCL_USE_DEPRECATED_OPENCL_1_1_APIS
                -DCL_USE_DEPRECATED_OPENCL_1_2_APIS
                -DCL_USE_DEPRECATED_OPENCL_2_0_APIS
                -DCL_USE_DEPRECATED_OPENCL_2_1_APIS
                -DCL_USE_DEPRECATED_OPENCL_2_2_APIS
)

if(WIN32 OR NOT DISABLE_WDDM_LINUX)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS -DWDDM_VERSION_NUMBER=23 -DNOMINMAX)
  add_definitions(-DLEGACY_D3DAPI_VERSION=20)
  if(NOT WIN32)
    add_definitions(-DWDDM_LINUX=1)
  endif()
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
if(UNIX)
  if(NEO_BUILD_DEBUG_SYMBOLS_PACKAGE)
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -g")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -g")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -g")
  endif()
  set(CMAKE_C_FLAGS_RELEASEINTERNAL "${CMAKE_C_FLAGS_RELEASEINTERNAL} -g")
  set(CMAKE_CXX_FLAGS_RELEASEINTERNAL "${CMAKE_CXX_FLAGS_RELEASEINTERNAL} -g")
  set(CMAKE_SHARED_LINKER_FLAGS_RELEASEINTERNAL "${CMAKE_SHARED_LINKER_FLAGS_RELEASEINTERNAL} -g")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASEINTERNAL "${CMAKE_EXE_LINKER_FLAGS_RELEASEINTERNAL} -g")
endif()

string(TOLOWER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_lower)
if("${BUILD_TYPE_lower}" STREQUAL "releaseinternal")
  add_definitions(-D_RELEASE_INTERNAL)
endif("${BUILD_TYPE_lower}" STREQUAL "releaseinternal")

message(STATUS "${CMAKE_BUILD_TYPE} build configuration")

set(NEO_RUNTIME_SUB_DIR "opencl/source")
set(NEO_RUNTIME_SOURCE_DIR "${NEO_SOURCE_DIR}/${NEO_RUNTIME_SUB_DIR}")

set(NEO_RUNTIME_TESTS_SUB_DIR "opencl/test/unit_test")
set(NEO_RUNTIME_TESTS_DIR "${NEO_SOURCE_DIR}/${NEO_RUNTIME_TESTS_SUB_DIR}")
set(NEO_OPENCL_BLACK_BOX_TESTS_SUB_DIR "opencl/test/black_box_test")
set(NEO_OPENCL_BLACK_BOX_TESTS_DIR "${NEO_SOURCE_DIR}/${NEO_OPENCL_BLACK_BOX_TESTS_SUB_DIR}")

# Set the ocloc directory
if(NOT DEFINED OCLOC_DIRECTORY)
  set(OCLOC_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/shared/offline_compiler)
endif()

# Set the shared source directory
if(NOT DEFINED NEO_SHARED_DIRECTORY)
  set(NEO_SHARED_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/shared/source)
endif()

# Set the shared test directory
if(NOT DEFINED NEO_SHARED_TEST_DIRECTORY)
  set(NEO_SHARED_TEST_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/shared/test)
endif()

if(SKIP_UNIT_TESTS)
  set(SKIP_NEO_UNIT_TESTS TRUE)
  set(SKIP_SHARED_UNIT_TESTS TRUE)
endif()

# Set our build directory
if(NOT DEFINED NEO_BUILD_DIR)
  set(NEO_BUILD_DIR ${CMAKE_BINARY_DIR})
endif()

if(NOT NEO_BINARY_DIR)
  set(NEO_BINARY_DIR ${CMAKE_BINARY_DIR})
endif()

# we use c++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# we force using response files
set(CMAKE_NINJA_FORCE_RESPONSE_FILE 1)

# set output paths
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${NEO_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${NEO_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${NEO_BINARY_DIR}/lib)

# determine Neo version
include(version.cmake)

# set variables for symbols stripping
if(UNIX AND NEO_BUILD_DEBUG_SYMBOLS_PACKAGE)
  set_property(GLOBAL PROPERTY DEBUG_SYMBOL_FILES "")
  set_property(GLOBAL PROPERTY IGDRCL_SYMBOL_FILE "")
  set_property(GLOBAL APPEND PROPERTY NEO_OCL_COMPONENTS_LIST "opencl-debuginfo")
  set(STRIP_SYMBOLS_TARGET strip_debug_symbols)
  set(DEBUG_SYMBOL_INSTALL_DIR "/usr/lib/debug${CMAKE_INSTALL_FULL_LIBDIR}")
  add_custom_target(${STRIP_SYMBOLS_TARGET} ALL)
endif()

# We want to build with the static, multithreaded runtime libraries (as opposed
# to the multithreaded runtime DLLs)
if(MSVC)
  string(REPLACE "/MDd" "/MTd" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
  string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASEINTERNAL "${CMAKE_CXX_FLAGS_RELEASEINTERNAL}")

  string(REPLACE "/GR" "/GR-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  add_definitions(-D_HAS_STATIC_RTTI=0)
  add_definitions(-DPURGE_DEBUG_KEY_NAMES=1)
else()
  add_definitions(-DPURGE_DEBUG_KEY_NAMES=0)
endif()

if(NOT SKIP_UNIT_TESTS)
  if(NOT DEFINED GTEST_REPEAT)
    set(GTEST_REPEAT 1)
  endif()
  message(STATUS "GTest repeat count set to ${GTEST_REPEAT}")

  if(NOT DEFINED GTEST_SHUFFLE)
    set(GTEST_SHUFFLE --gtest_shuffle --gtest_random_seed=0)
  endif()
  message(STATUS "GTest shuffle set to ${GTEST_SHUFFLE}")
endif()

if(NOT KMDAF_HEADERS_DIR)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../kmdaf/inc/common/kmDaf.h")
    get_filename_component(KMDAF_HEADERS_DIR "../kmdaf/inc/common/" ABSOLUTE)
    message(STATUS "KM-DAF headers dir: ${KMDAF_HEADERS_DIR}")
  endif()
endif()

if(NOT DEFINED SOURCE_LEVEL_DEBUGGER_HEADERS_DIR)
  get_filename_component(SOURCE_LEVEL_DEBUGGER_HEADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/source_level_debugger/" ABSOLUTE)
  message(STATUS "Source Level Debugger headers dir: ${SOURCE_LEVEL_DEBUGGER_HEADERS_DIR}")
endif()

get_filename_component(AUB_STREAM_HEADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party${BRANCH_DIR_SUFFIX}/aub_stream/headers" ABSOLUTE)
if(IS_DIRECTORY ${AUB_STREAM_HEADERS_DIR})
  message(STATUS "Aub Stream Headers dir: ${AUB_STREAM_HEADERS_DIR}")
else()
  message(FATAL_ERROR "Aub Stream headers not available!")
endif()

if(TARGET aub_stream)
  set(AUB_STREAM_PROJECT_NAME "aub_stream")
else()
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
endif()

# Metrics Library
get_filename_component(METRICS_LIBRARY_HEADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party${BRANCH_DIR_SUFFIX}/metrics_library" ABSOLUTE)
if(IS_DIRECTORY ${METRICS_LIBRARY_HEADERS_DIR})
  message(STATUS "Metrics Library dir: ${METRICS_LIBRARY_HEADERS_DIR}")
  include_directories(${NEO_SOURCE_DIR}/third_party${BRANCH_DIR_SUFFIX}/metrics_library)
else()
  message(FATAL_ERROR "Metrics Library headers not available!")
endif()

# Metrics Discovery
get_filename_component(METRICS_DISCOVERY_HEADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party${BRANCH_DIR_SUFFIX}/metrics_discovery" ABSOLUTE)
if(IS_DIRECTORY ${METRICS_DISCOVERY_HEADERS_DIR})
  message(STATUS "Metrics Discovery dir: ${METRICS_DISCOVERY_HEADERS_DIR}")
  include_directories(${NEO_SOURCE_DIR}/third_party${BRANCH_DIR_SUFFIX}/metrics_discovery)
else()
  message(FATAL_ERROR "Metrics Discovery headers not available!")
endif()

if(SUPPORT_DG1 AND "${BRANCH_TYPE}" STREQUAL "")
  get_filename_component(I915_INCLUDES_DIR "${NEO_SOURCE_DIR}/third_party/uapi/dg1" ABSOLUTE)
else()
  get_filename_component(I915_INCLUDES_DIR "${NEO_SOURCE_DIR}/third_party${BRANCH_DIR_SUFFIX}/uapi" ABSOLUTE)
endif()
message(STATUS "i915 includes dir: ${I915_INCLUDES_DIR}")

if(NOT DEFINED KHRONOS_HEADERS_DIR)
  get_filename_component(DIR_tmp "${CMAKE_CURRENT_SOURCE_DIR}/third_party/opencl_headers" ABSOLUTE)
  if(IS_DIRECTORY ${DIR_tmp})
    set(KHRONOS_HEADERS_DIR ${DIR_tmp})
    add_definitions(-DCL_TARGET_OPENCL_VERSION=300)
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
endif()
message(STATUS "Third party dir: ${THIRD_PARTY_DIR}")

if(WIN32)
  if(${WindowsTargetPlatformVersion} VERSION_LESS "10.0.18328.0")
    set(CONST_FROM_WDK_10_0_18328_0)
  else()
    set(CONST_FROM_WDK_10_0_18328_0 "CONST")
  endif()
  add_compile_options(-DCONST_FROM_WDK_10_0_18328_0=${CONST_FROM_WDK_10_0_18328_0})
  set(WDK_INCLUDE_PATHS "")
  list(APPEND WDK_INCLUDE_PATHS
       "${WDK_DIR}/Include/${WindowsTargetPlatformVersion}/um"
       "${WDK_DIR}/Include/${WindowsTargetPlatformVersion}/shared"
       "${WDK_DIR}/Include/${WindowsTargetPlatformVersion}/km"
  )
  message(STATUS "WDK include paths: ${WDK_INCLUDE_PATHS}")
elseif(NOT DISABLE_WDDM_LINUX)
  add_compile_options(-DCONST_FROM_WDK_10_0_18328_0=CONST)
  add_compile_options(-DWDDM_LINUX=1)
  set(WDK_INCLUDE_PATHS "")
  get_filename_component(DX_HEADERS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/DirectX-Headers" ABSOLUTE)
  list(APPEND WDK_INCLUDE_PATHS ${DX_HEADERS_PATH}/include/wsl)
  list(APPEND WDK_INCLUDE_PATHS ${DX_HEADERS_PATH}/include/wsl/stubs)
  list(APPEND WDK_INCLUDE_PATHS ${DX_HEADERS_PATH}/include/directx)
  list(APPEND WDK_INCLUDE_PATHS ${DX_HEADERS_PATH}/include/dxguids)
  list(APPEND WDK_INCLUDE_PATHS ${D3DKMTHK_INCLUDE_PATHS})
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
    pkg_check_modules(NEO__IGC igc-opencl)
    if(DEFINED __tmp_LIBDIR)
      set(ENV{PKG_CONFIG_PATH} ${OLD_PKG_CONFIG_PATH})
      set(NEO__IGC_LIBRARY_PATH "${IGC_DIR}/${__tmp_LIBDIR}/")
    endif()

    if(NEO__IGC_FOUND)
      if(DEFINED IGC_DIR AND IS_DIRECTORY "${IGC_DIR}/${__tmp_LIBDIR}/pkgconfig/")
        string(REPLACE "${NEO__IGC_INCLUDEDIR}" "${IGC_DIR}/include/igc" NEO__IGC_INCLUDE_DIRS "${NEO__IGC_INCLUDE_DIRS}")
      endif()

      set(NEO__IGC_INCLUDE_DIR ${NEO__IGC_INCLUDE_DIRS})
      message(STATUS "IGC include dirs: ${NEO__IGC_INCLUDE_DIR}")
    endif()
    if(DEFINED __tmp_LIBDIR)
      unset(__tmp_LIBDIR)
    endif()
  endif()

  if(NEO__IGC_FOUND)
    # do nothing
  elseif(EXISTS "${IGC_DIR}/CMakeLists.txt")
    message(STATUS "IGC source dir is: ${IGC_DIR}")

    set(IGC_OPTION__OUTPUT_DIR "${NEO_BUILD_DIR}/igc")
    set(IGC_OPTION__INCLUDE_IGC_COMPILER_TOOLS OFF)
    add_subdirectory_unique("${IGC_DIR}" "${NEO_BUILD_DIR}/igc" EXCLUDE_FROM_ALL)

    set(NEO__IGC_TARGETS "${IGC__IGC_TARGETS}")
    foreach(TARGET_tmp ${NEO__IGC_TARGETS})
      list(APPEND NEO__IGC_INCLUDE_DIR $<TARGET_PROPERTY:${TARGET_tmp},INTERFACE_INCLUDE_DIRECTORIES>)
      list(APPEND NEO__IGC_COMPILE_DEFINITIONS $<TARGET_PROPERTY:${TARGET_tmp},INTERFACE_COMPILE_DEFINITIONS>)
    endforeach()
    message(STATUS "IGC targets: ${NEO__IGC_TARGETS}")
  else()
    message(FATAL_ERROR "Intel Graphics Compiler not found!")
  endif()
else()
  set(NEO__IGC_TARGETS "${IGC__IGC_TARGETS}")
  foreach(TARGET_tmp ${NEO__IGC_TARGETS})
    list(APPEND NEO__IGC_INCLUDE_DIR $<TARGET_PROPERTY:${TARGET_tmp},INTERFACE_INCLUDE_DIRECTORIES>)
    list(APPEND NEO__IGC_LIBRARY_PATH $<TARGET_FILE_DIR:${TARGET_tmp}>)
  endforeach()
  string(REPLACE ";" ":" NEO__IGC_LIBRARY_PATH "${NEO__IGC_LIBRARY_PATH}")
  message(STATUS "IGC targets: ${NEO__IGC_TARGETS}")
endif()

# VISA headers - always relative to IGC
if(IS_DIRECTORY "${IGC_DIR}/../visa")
  get_filename_component(VISA_DIR "${IGC_DIR}/../visa" ABSOLUTE)
elseif(IS_DIRECTORY "${IGC_DIR}/visa")
  set(VISA_DIR "${IGC_DIR}/visa")
elseif(IS_DIRECTORY "${IGC_DIR}/include/visa")
  set(VISA_DIR "${IGC_DIR}/include/visa")
elseif(IS_DIRECTORY "${NEO__IGC_INCLUDEDIR}/../visa")
  get_filename_component(VISA_DIR "${NEO__IGC_INCLUDEDIR}/../visa" ABSOLUTE)
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
if(TARGET igfx_gmmumd_dll)
  set(GMM_TARGET_NAME "igfx_gmmumd_dll")
  set(GMM_LINK_NAME $<TARGET_PROPERTY:${GMM_TARGET_NAME},OUTPUT_NAME>)
  set(NEO__GMM_LIBRARY_PATH $<TARGET_FILE_DIR:${GMM_TARGET_NAME}>)
  set(NEO__GMM_INCLUDE_DIR $<TARGET_PROPERTY:${GMM_TARGET_NAME},INTERFACE_INCLUDE_DIRECTORIES>)
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
    pkg_check_modules(NEO__GMM igdgmm)
    if(DEFINED __tmp_LIBDIR)
      set(ENV{PKG_CONFIG_PATH} ${OLD_PKG_CONFIG_PATH})
    endif()

    if(NEO__GMM_FOUND)
      if(DEFINED __tmp_LIBDIR)
        string(REPLACE "${NEO__GMM_INCLUDEDIR}" "${GMM_DIR}/include/igdgmm" NEO__GMM_INCLUDE_DIRS "${NEO__GMM_INCLUDE_DIRS}")
        string(REPLACE "${NEO__GMM_LIBDIR}" "${GMM_DIR}/${__tmp_LIBDIR}" NEO__GMM_LIBDIR "${NEO__GMM_LIBDIR}")
        set(NEO__GMM_LIBRARY_PATH "${NEO__GMM_LIBDIR}")
      endif()

      set(GMM_TARGET_NAME "igfx_gmmumd_dll")
      set(GMM_LINK_NAME ${NEO__GMM_LIBRARIES})

      set(NEO__GMM_INCLUDE_DIR ${NEO__GMM_INCLUDE_DIRS})
      message(STATUS "GmmLib include dirs: ${NEO__GMM_INCLUDE_DIR}")
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

      if(NOT DEFINED GMM_TARGET_NAME)
        set(GMM_TARGET_NAME "igfx_gmmumd_dll")
      endif()

      set(NEO__GMM_INCLUDE_DIR $<TARGET_PROPERTY:${GMM_TARGET_NAME},INTERFACE_INCLUDE_DIRECTORIES>)
      set(NEO__GMM_LIBRARY_PATH $<TARGET_FILE_DIR:${GMM_TARGET_NAME}>)
      set(GMM_LINK_NAME $<TARGET_PROPERTY:${GMM_TARGET_NAME},OUTPUT_NAME>)
    else()
      message(FATAL_ERROR "GmmLib not found!")
    endif()
  endif()
endif()

macro(copy_gmm_dll_for target)
  if(NOT UNIX)
    add_custom_command(
                       TARGET ${target}
                       POST_BUILD
                       COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${GMM_TARGET_NAME}> $<TARGET_FILE_DIR:${target}>
    )
  endif()
endmacro()

link_directories(${NEO__GMM_LIBRARY_PATH})

if(NOT DISABLE_LIBVA)
  # LibVA detection
  if(UNIX)
    set(NEO__LIBVA_IS_REQUIRED "")
    if(IGDRCL_FORCE_USE_LIBVA)
      set(NEO__LIBVA_IS_REQUIRED "REQUIRED")
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
    pkg_check_modules(NEO__LIBVA ${NEO__LIBVA_IS_REQUIRED} libva>=1.0.0)
    include(CheckLibraryExists)
    if(DEFINED LIBVA_SOURCE_DIR AND IS_DIRECTORY "${LIBVA_SOURCE_DIR}/lib/pkgconfig/")
      set(ENV{PKG_CONFIG_PATH} ${OLD_PKG_CONFIG_PATH})
    endif()

    if(NEO__LIBVA_FOUND)
      CHECK_LIBRARY_EXISTS(va vaGetLibFunc ${NEO__LIBVA_LIBDIR} HAVE_VAGETLIBFUNC)

      add_definitions(-DLIBVA)
      message(STATUS "Using libva ")

      if(DEFINED LIBVA_SOURCE_DIR AND IS_DIRECTORY "${LIBVA_SOURCE_DIR}/lib/pkgconfig/")
        string(REPLACE "${NEO__LIBVA_INCLUDEDIR}" "${LIBVA_SOURCE_DIR}/include" NEO__LIBVA_INCLUDE_DIRS "${NEO__LIBVA_INCLUDE_DIRS}")
        string(REPLACE "${NEO__LIBVA_LIBDIR}" "${LIBVA_SOURCE_DIR}/lib" NEO__LIBVA_LIBDIR "${NEO__LIBVA_LIBDIR}")
        set(NEO__LIBVA_LIBRARY_PATH "${NEO__LIBVA_LIBDIR}")
      endif()

      list(LENGTH NEO__LIBVA_INCLUDE_DIRS __len)
      if(__len GREATER 0)
        set(NEO__LIBVA_INCLUDE_DIR ${NEO__LIBVA_INCLUDE_DIRS})
        include_directories("${NEO__LIBVA_INCLUDE_DIR}")
        message(STATUS "LibVA include dirs: ${NEO__LIBVA_INCLUDE_DIR}")
      endif()
    endif()
  endif()
endif()

set(SLD_LIBRARY_NAME "igfxdbgxchg")
if(NEO_BITS STREQUAL "64")
  set(SLD_LIBRARY_NAME "${SLD_LIBRARY_NAME}${NEO_BITS}")
endif()

add_subdirectory_unique(third_party/gtest)

message(STATUS "AUB_STREAM_DIR = ${AUB_STREAM_DIR}")
if(DEFINED AUB_STREAM_DIR)
  set(AUB_STREAM_PROJECT_NAME "aub_stream")
  add_subdirectory_unique(${AUB_STREAM_DIR} ${CMAKE_BINARY_DIR}/aub_stream EXCLUDE_FROM_ALL)
  set(ENGINE_NODE_DIR ${AUB_STREAM_DIR}${BRANCH_DIR_SUFFIX}/headers)
else()
  set(ENGINE_NODE_DIR ${AUB_STREAM_HEADERS_DIR})
endif()
message(STATUS "Engine node dir: ${ENGINE_NODE_DIR}")

add_definitions(-DGMM_OCL)

# We want to organize our IDE targets into folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Get available platfroms
include(platforms.cmake)

# Enable/Disable BuiltIns compilation during build
set(COMPILE_BUILT_INS TRUE CACHE BOOL "Enable built-in kernels compilation")

# Changing the default executable and library output directories
set(IGDRCL_OUTPUT_DIR "${IGDRCL_OPTION__OUTPUT_DIR}")

# do not add rpath
set(CMAKE_SKIP_RPATH YES CACHE BOOL "" FORCE)

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
if(KMD_PROFILING)
  add_definitions(-DKMD_PROFILING=${KMD_PROFILING})
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

  # Disable COMDAT folding optimization
  set(LINKER_FLAGS "${LINKER_FLAGS} /OPT:NOICF")

  # Use legacy float rounding
  set(LINKER_FLAGS "${LINKER_FLAGS} legacy_stdio_float_rounding.obj")

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
      set(GTEST_ENV ${GTEST_ENV} LSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/opencl/test/unit_test/lsan_suppressions.txt)
    else()
      message(STATUS "Address sanitization with clang not yet support")
    endif()
  endif()
  if(USE_TSAN)
    if(CMAKE_COMPILER_IS_GNUCC AND USE_ASAN)
      message(STATUS "Cannot use thread sanitization with address sanitization in gcc")
    else()
      set(TSAN_FLAGS " -fsanitize=thread -DSANITIZER_BUILD")
      set(TSAN_LIBS "tsan")
    endif()
  endif()

endif()

# setup variables needed for custom configuration type
# generate PDB files even for release build on MSVC
if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
  set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF")

  set(CMAKE_CXX_FLAGS_RELEASEINTERNAL "${CMAKE_CXX_FLAGS_RELEASEINTERNAL} /Zi")
  set(CMAKE_SHARED_LINKER_FLAGS_RELEASEINTERNAL "${CMAKE_SHARED_LINKER_FLAGS_RELEASEINTERNAL} /DEBUG /OPT:REF")

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
  if(NOT NEO_DISABLE_MITIGATIONS)
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
  else()
    message(WARNING "Spectre mitigation DISABLED")
  endif()
endif()

if(NOT MSVC)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-depth=1024")
endif()

# Compiler warning flags
if(NOT MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wempty-body")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wignored-qualifiers")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wtype-limits")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wuninitialized")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra -Wno-unused-parameter -Wno-missing-field-initializers")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Winvalid-pch")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-pragmas")

  if("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
    # clang only
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wshorten-64-to-32 -Wno-extern-c-compat")
    if(NOT (CMAKE_C_COMPILER_VERSION VERSION_LESS 3.6))
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-local-typedefs -DSANITIZER_BUILD")
    endif()
    if(NOT (CMAKE_C_COMPILER_VERSION VERSION_LESS 4.0))
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-register") # Added for htons()
    endif()
    if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 10.0)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-copy")
    endif()
  else()
    # gcc only
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-local-typedefs -Wno-unused-but-set-variable")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wclobbered")
    if(CMAKE_C_COMPILER_VERSION VERSION_LESS 7.0)
    else()
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wimplicit-fallthrough=4")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-noexcept-type") # Added for gtest
    endif()
  endif()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")

  if(USE_SANITIZE_UB)
    check_cxx_compiler_flag(-fsanitize=undefined COMPILER_SUPPORTS_UNDEFINED_BEHAVIOR_SANITIZER)
    if(COMPILER_SUPPORTS_UNDEFINED_BEHAVIOR_SANITIZER)
      message(STATUS "Enabling undefined behavior sanitizer")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fsanitize-recover=vptr -fno-rtti -DSANITIZER_BUILD")
      if(NOT SANITIZE_UB_ALLOW_CONTINUE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-sanitize-recover=undefined")
      endif()
    endif()
  endif()
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
      if(CMAKE_C_COMPILER_VERSION VERSION_LESS 4.9)
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
include_directories(${NEO_SHARED_DIRECTORY}/aub_mem_dump/definitions${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SHARED_DIRECTORY}/built_ins/builtinops${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SHARED_DIRECTORY}/command_stream/definitions${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SHARED_DIRECTORY}/compiler_interface/compiler_options${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SHARED_DIRECTORY}/debug_settings/definitions${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SHARED_DIRECTORY}/dll/devices${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SHARED_DIRECTORY}/gen_common${BRANCH_DIR_SUFFIX})
if(WIN32)
  include_directories(${NEO_SHARED_DIRECTORY}/gmm_helper/windows/gmm_memory${BRANCH_DIR_SUFFIX})
else()
  include_directories(${NEO_SHARED_DIRECTORY}/gmm_helper/windows/gmm_memory)
endif()
include_directories(${NEO_SHARED_DIRECTORY}/helpers/definitions${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SHARED_DIRECTORY}/memory_properties${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SHARED_DIRECTORY}/sku_info/definitions${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SOURCE_DIR}/opencl/source/command_queue/definitions${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SOURCE_DIR}/opencl/source/mem_obj/definitions${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SOURCE_DIR}/opencl/source/memory_manager/definitions${BRANCH_DIR_SUFFIX})
if(MSVC)
  include_directories(${NEO_SOURCE_DIR}/opencl/source/sharings/gl/windows/include)
else()
  include_directories(${NEO_SHARED_DIRECTORY}/os_interface/linux/flags${BRANCH_DIR_SUFFIX})
endif()
if(SUPPORT_XE_HP_CORE)
  include_directories(${NEO_SHARED_DIRECTORY}/xe_hp_core/definitions${BRANCH_DIR_SUFFIX})
  include_directories(${NEO_SOURCE_DIR}/opencl/source/xe_hp_core/definitions${BRANCH_DIR_SUFFIX})
endif()
# Define where to put binaries
set(TargetDir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
if(MSVC)
  if(NOT "${CMAKE_GENERATOR}" STREQUAL "Ninja")
    set(TargetDir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE})
  endif()
endif()

add_subdirectory_unique(${OCLOC_DIRECTORY} ${NEO_BUILD_DIR}/offline_compiler)
target_compile_definitions(ocloc_lib PRIVATE MOCKABLE_VIRTUAL=)

if(DONT_CARE_OF_VIRTUALS)
  set(NEO_SHARED_RELEASE_LIB_NAME "neo_shared")
  set(NEO_SHARED_MOCKABLE_LIB_NAME ${NEO_SHARED_RELEASE_LIB_NAME})
else()
  set(NEO_SHARED_RELEASE_LIB_NAME "neo_shared")
  if(NOT SKIP_UNIT_TESTS)
    set(NEO_SHARED_MOCKABLE_LIB_NAME "neo_shared_mockable")
  endif()
endif()

set(BIKSIM_LIB_NAME "biksim")
set(BUILTINS_SOURCES_LIB_NAME "builtins_sources")
set(BUILTINS_BINARIES_BINDFUL_LIB_NAME "builtins_binaries_bindful")
set(BUILTINS_BINARIES_BINDLESS_LIB_NAME "builtins_binaries_bindless")
set(BUILTINS_SPIRV_LIB_NAME "builtins_spirv")
set(BUILTINS_VME_LIB_NAME "builtins_vme")
set(SCHEDULER_BINARY_LIB_NAME "scheduler_binary")

add_subdirectory_unique(shared/source)
add_subdirectory_unique(shared/generate_cpp_array)

macro(generate_runtime_lib LIB_NAME MOCKABLE GENERATE_EXEC)
  set(NEO_STATIC_LIB_NAME ${LIB_NAME})
  set(SHARINGS_ENABLE_LIB_NAME "${LIB_NAME}_sharings_enable")
  set(GENERATE_EXECUTABLE ${GENERATE_EXEC})

  if(${MOCKABLE})
    set(NEO_SHARED_LIB ${NEO_SHARED_MOCKABLE_LIB_NAME})
  else()
    set(NEO_SHARED_LIB ${NEO_SHARED_RELEASE_LIB_NAME})
  endif()
  if(NOT BUILD_WITHOUT_RUNTIME)
    add_subdirectory(${NEO_RUNTIME_SUB_DIR} "${NEO_BUILD_DIR}/${LIB_NAME}")
  else()
    add_subdirectory(${NEO_RUNTIME_SUB_DIR} "${NEO_BUILD_DIR}/${LIB_NAME}" EXCLUDE_FROM_ALL)
  endif()
  target_compile_definitions(${BUILTINS_SOURCES_LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=)
  target_compile_definitions(${BUILTINS_VME_LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=)
  target_compile_definitions(${BUILTINS_BINARIES_BINDFUL_LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=)
  target_compile_definitions(${BUILTINS_BINARIES_BINDLESS_LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=)
  target_compile_definitions(${BUILTINS_SPIRV_LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=)
  target_compile_definitions(${SCHEDULER_BINARY_LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=)

  if(${MOCKABLE})
    target_compile_definitions(${LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=virtual)
    target_compile_definitions(${SHARINGS_ENABLE_LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=virtual)
    target_compile_definitions(${LIB_NAME} PUBLIC DEFAULT_TEST_PLATFORM=${DEFAULT_TESTED_PLATFORM} DEFAULT_TEST_FAMILY_NAME=${DEFAULT_TESTED_FAMILY_NAME} ${TESTED_GEN_FLAGS_DEFINITONS})
  else()
    target_compile_definitions(${LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=)
    target_compile_definitions(${SHARINGS_ENABLE_LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=)
  endif()
endmacro()

set(NEO_DYNAMIC_LIB_NAME "igdrcl_dll") # single NEO dll
set(NEO_DLL_NAME_BASE "igdrcl")

if(DONT_CARE_OF_VIRTUALS)
  message(STATUS "All targets will use virtuals")

  set(NEO_RELEASE_LIB_NAME "igdrcl_lib")
  set(NEO_MOCKABLE_LIB_NAME ${NEO_RELEASE_LIB_NAME})
  generate_runtime_lib(${NEO_RELEASE_LIB_NAME} TRUE TRUE)
else()
  set(NEO_RELEASE_LIB_NAME "igdrcl_lib_release") # Used by dll/so
  generate_runtime_lib(${NEO_RELEASE_LIB_NAME} FALSE TRUE)
  if(NOT SKIP_UNIT_TESTS)
    set(NEO_MOCKABLE_LIB_NAME "igdrcl_lib_mockable") # Used by ULTS
    generate_runtime_lib(${NEO_MOCKABLE_LIB_NAME} TRUE FALSE)
  endif()
endif()

set(NEO_STATICALLY_LINKED_LIBRARIES ${NEO_RELEASE_LIB_NAME} ${NEO_SHARED_RELEASE_LIB_NAME} ${NEO_RELEASE_LIB_NAME} ${NEO_SHARED_RELEASE_LIB_NAME})
set(NEO_STATICALLY_LINKED_LIBRARIES_MOCKABLE ${NEO_MOCKABLE_LIB_NAME} ${NEO_SHARED_MOCKABLE_LIB_NAME} ${NEO_MOCKABLE_LIB_NAME} ${NEO_SHARED_MOCKABLE_LIB_NAME})

if(NOT SKIP_UNIT_TESTS)
  include(${CMAKE_CURRENT_SOURCE_DIR}/level_zero/cmake/${BRANCH_TYPE}/l0_tests.cmake)
endif()

add_subdirectory_unique(shared/test/unit_test)

if(DEFAULT_TESTED_PLATFORM AND NOT SKIP_NEO_UNIT_TESTS)
  add_subdirectory_unique(${NEO_RUNTIME_TESTS_SUB_DIR} ${NEO_BUILD_DIR}/${NEO_RUNTIME_TESTS_SUB_DIR})
else()
  hide_subdir(${NEO_RUNTIME_TESTS_SUB_DIR})
endif()

if(NOT BUILD_WITHOUT_RUNTIME)
  add_subdirectory_unique(${NEO_OPENCL_BLACK_BOX_TESTS_SUB_DIR})
else()
  hide_subdir(${NEO_OPENCL_BLACK_BOX_TESTS_SUB_DIR})
endif()

set(DONT_LINK_ELF_STATICALLY TRUE)
if(EXISTS ${NEO_SOURCE_DIR}/../internal)
  add_subdirectory_unique(${NEO_SOURCE_DIR}/../internal ${NEO_BUILD_DIR}/internal)
endif()

set(NEO_SCRIPT_PROJECTS_FOLDER "neo scripts")
add_subdirectory_unique(scripts/lint)
if(EXISTS ${NEO_SOURCE_DIR}/scripts/format)
  add_subdirectory_unique(scripts/format)
endif()

configure_file(config.h.in ${NEO_BUILD_DIR}/config.h)
if(WIN32 AND NOT NEO__IGC_FOUND)
  configure_file(igc.opencl.h.in ${NEO_BUILD_DIR}/igc.opencl.h)
endif()
configure_file(driver_version.h.in ${NEO_BUILD_DIR}/driver_version.h) # Put Driver version into define
configure_file(lib_names.h.in ${NEO_BUILD_DIR}/lib_names.h)

hide_subdir(${NEO_RUNTIME_SUB_DIR})

if(BUILD_WITH_L0)
  add_subdirectory_unique(level_zero)
endif()

add_subdirectory_unique(target_unit_tests)
add_subdirectory_unique(target_aub_tests)

add_subdirectories()

include(package.cmake)
