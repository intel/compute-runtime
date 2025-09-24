#
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.13.0 FATAL_ERROR)

if(NEO_ENABLE_INTERPROCEDURAL_OPTIMIZATION)
  cmake_policy(SET CMP0069 NEW)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
  set(NEO_DISABLE_LD_LLD TRUE)
  message(STATUS "Enabling interprocedural optimization")
endif()

# Set the runtime source directory
if(NOT DEFINED NEO_SOURCE_DIR)
  set(NEO_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif()

if(CMAKE_GENERATOR MATCHES "Visual Studio")
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
  set(BRANCH_DIR_SUFFIX "${BRANCH_DIR_SUFFIX}/")
endif()
message(STATUS "branch dir list: ${BRANCH_DIR_LIST}")

if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
  set(NEO_TARGET_PROCESSOR "x86_64")
elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "AMD64")
  set(NEO_TARGET_PROCESSOR "x86_64")
elseif(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64")
  set(NEO_TARGET_PROCESSOR "aarch64")
  if(NOT ${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL ${CMAKE_SYSTEM_PROCESSOR})
    set(NEO_DISABLE_LD_LLD TRUE)
    set(NEO_DISABLE_LD_GOLD TRUE)
  endif()
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/sse2neon)
  set(DISABLE_WDDM_LINUX TRUE)
endif()
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/shared/source/gmm_helper/igfxfmid_wrapper/${BRANCH_DIR_SUFFIX}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/shared/source/device_binary_format/aot_platforms/${BRANCH_DIR_SUFFIX}")

message(STATUS "Host processor: ${CMAKE_HOST_SYSTEM_PROCESSOR}")
message(STATUS "Target processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Neo target processor: ${NEO_TARGET_PROCESSOR}")

if(NOT DEFINED NEO_TARGET_PROCESSOR)
  message(FATAL_ERROR "Unsupported target processor: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

# Include custom configs
include("${BRANCH_TYPE}/custom_config.cmake" OPTIONAL)

# Include needed macros
include(cmake/common_macros.cmake)

if(CMAKE_CONFIGURATION_TYPES)
  # Set custom build types for multi-configuration generators
  set(CMAKE_CONFIGURATION_TYPES "Release;ReleaseInternal;Debug")
  if(DEFINED CMAKE_BUILD_TYPE)
    list(APPEND CMAKE_CONFIGURATION_TYPES ${CMAKE_BUILD_TYPE})
  endif()
  list(REMOVE_DUPLICATES CMAKE_CONFIGURATION_TYPES)
endif()

include(GNUInstallDirs)

if(USE_SANITIZE_UB AND WIN32)
  add_definitions(-D_ITERATOR_DEBUG_LEVEL=0)
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Xlinker /NODEFAULTLIB:clang_rt.ubsan_standalone_cxx-x86_64.lib")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Xlinker /NODEFAULTLIB:clang_rt.ubsan_standalone_cxx-x86_64.lib")
endif()

if(WIN32 AND NOT MSVC)
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Xlinker onecore.lib")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Xlinker onecore.lib")
endif()

if(NOT DEFINED NEO_USE_CL_CACHE)
  set(NEO_USE_CL_CACHE FALSE CACHE BOOL "use cl_cache when running ocloc" FORCE)
endif()
if(NEO_USE_CL_CACHE)
  set(NEO_DISABLE_BUILTINS_COMPILATION FALSE)
endif()
# define global property where we will collect component list to package
define_property(GLOBAL PROPERTY NEO_OCL_COMPONENTS_LIST BRIEF_DOCS "List of components" FULL_DOCS "List of components to create packages")
set(GLOBAL PROPERTY NEO_OCL_COMPONENTS_LIST "")
define_property(GLOBAL PROPERTY NEO_L0_COMPONENTS_LIST BRIEF_DOCS "List of components" FULL_DOCS "List of components to create packages")
set(GLOBAL PROPERTY NEO_L0_COMPONENTS_LIST "")

string(REPLACE " " ";" NEO_RUN_INTERCEPTOR_LIST "${NEO_RUN_INTERCEPTOR}")

if(TR_DEPRECATED)
  add_definitions(-D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING=1)
endif()

if(RELEASE_WITH_REGKEYS)
  message(STATUS "Enabling RegKey reading in release build")
  add_definitions(-D_RELEASE_BUILD_WITH_REGKEYS)
endif()

if(ENABLE_DYNAMIC_MEMORY_TRACKING)
  message(STATUS "Enabling dynamic memory tracking")
  add_definitions(-DENABLE_DYNAMIC_MEMORY_TRACKING)
endif()

if(NEO_SERIALIZED_BUILTINS_COMPILATION)
  message(STATUS "Enabling serialized builtins compilation")
endif()

if(DEFINED NEO_ENABLE_ALARM_MODE AND "${NEO_ENABLE_ALARM_MODE}" STREQUAL "0")
  message(STATUS "Disabling alarm in tests")
  add_definitions(-DENABLE_ALARM_DEFAULT=0)
else()
  add_definitions(-DENABLE_ALARM_DEFAULT=1)
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

if(NOT DEFINED NEO_BUILD_WITH_OCL)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/opencl/CMakeLists.txt)
    set(NEO_BUILD_WITH_OCL TRUE)
  endif()
endif()

if(NOT DEFINED BUILD_WITH_L0)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/level_zero/CMakeLists.txt)
    if("${NEO_BITS}" STREQUAL "64")
      set(BUILD_WITH_L0 TRUE)
    endif()
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

add_definitions(-D__user= -D__packed=)

set(CMAKE_C_FLAGS_RELEASEINTERNAL "${CMAKE_C_FLAGS_RELEASE}")
set(CMAKE_CXX_FLAGS_RELEASEINTERNAL "${CMAKE_CXX_FLAGS_RELEASE}")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASEINTERNAL "${CMAKE_SHARED_LINKER_FLAGS_RELEASE}")
set(CMAKE_EXE_LINKER_FLAGS_RELEASEINTERNAL "${CMAKE_EXE_LINKER_FLAGS_RELEASE}")

include(CheckCXXCompilerFlag)
if(UNIX)
  if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU" AND(
                                                   ${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS 9.0 OR
                                                   (${CMAKE_CXX_COMPILER_VERSION} VERSION_GREATER_EQUAL 12.0 AND ${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS 13.0
    )))
    message(STATUS "Disabling LTO because compiler is ${CMAKE_CXX_COMPILER_ID} v${CMAKE_CXX_COMPILER_VERSION}")
    set(NEO_DISABLE_LTO TRUE)
  endif()

  if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU" AND ${CMAKE_CXX_COMPILER_VERSION} VERSION_GREATER_EQUAL 14.0)
    message(STATUS "Disabling LTO because compiler is ${CMAKE_CXX_COMPILER_ID} v${CMAKE_CXX_COMPILER_VERSION}")
    set(NEO_DISABLE_LTO TRUE)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)
  endif()

  if(NOT NEO_DISABLE_LTO)
    check_cxx_compiler_flag("-flto=auto" COMPILER_SUPPORTS_FLTO_AUTO)
    if(COMPILER_SUPPORTS_FLTO_AUTO)
      message(STATUS "enabling lto=auto, disabling ld.lld")
      set(NEO_DISABLE_LD_LLD TRUE)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto=auto")
      check_cxx_compiler_flag("-ffat-lto-objects" COMPILER_SUPPORTS_FAT_LTO_OBJECTS)
      if(COMPILER_SUPPORTS_FAT_LTO_OBJECTS)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffat-lto-objects")
      endif()
    endif()
  endif()
  find_program(__LD_GOLD_FOUND ld.gold)
  find_program(__LD_LLD_FOUND ld.lld)
  check_cxx_compiler_flag(-fuse-ld=lld COMPILER_SUPPORTS_LLD)
  check_cxx_compiler_flag(-fuse-ld=gold COMPILER_SUPPORTS_GOLD)
  if(NOT NEO_DISABLE_LD_LLD AND COMPILER_SUPPORTS_LLD AND __LD_LLD_FOUND)
    message(STATUS "Using linker: ld.lld")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld -Wl,--build-id=sha1")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=lld -Wl,--build-id=sha1")
  elseif(NOT NEO_DISABLE_LD_GOLD AND COMPILER_SUPPORTS_GOLD AND __LD_GOLD_FOUND)
    message(STATUS "Using linker: ld.gold")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=gold")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=gold")
  else()
    message(STATUS "Using linker: default")
  endif()
  if(ENABLE_DYNAMIC_MEMORY_TRACKING)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--export-dynamic")
  endif()
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

# leave temporarily for compatibitlity
if(DEFINED SKIP_UNIT_TESTS)
  set(NEO_SKIP_UNIT_TESTS ${SKIP_UNIT_TESTS})
endif()

if(NEO_SKIP_UNIT_TESTS)
  set(NEO_SKIP_SHARED_UNIT_TESTS TRUE)
  set(NEO_SKIP_OCL_UNIT_TESTS TRUE)
  set(NEO_SKIP_L0_UNIT_TESTS TRUE)
  set(NEO_SKIP_L0_BLACK_BOX_TESTS TRUE)
  set(NEO_SKIP_MT_UNIT_TESTS TRUE)
endif()

if(DEFINED NEO_GENERATE_AUBS_FOR)
  if(DEFINED NEO_SKIP_AUB_TESTS_RUN AND NEO_SKIP_AUB_TESTS_RUN)
    message(FATAL_ERROR "Invalid cmake flags combination, NEO_GENERATE_AUBS_FOR assumes aub tests execution is not skipped")
  endif()
  set(NEO_SKIP_AUB_TESTS_RUN FALSE)
endif()

if(NOT DEFINED NEO_SKIP_AUB_TESTS_RUN)
  set(NEO_SKIP_AUB_TESTS_RUN TRUE)
endif()

if(NOT DEFINED NEO_SKIP_AUB_TESTS)
  set(NEO_SKIP_AUB_TESTS TRUE)
endif()

if(NOT NEO_SKIP_AUB_TESTS_RUN)
  set(NEO_SKIP_AUB_TESTS FALSE)
endif()

if(NEO_IGNORE_INVALID_TEST_EXCLUDES)
  add_definitions(-DNEO_IGNORE_INVALID_TEST_EXCLUDES)
endif()

message(STATUS "NEO_SKIP_AUB_TESTS: ${NEO_SKIP_AUB_TESTS}")
message(STATUS "NEO_SKIP_AUB_TESTS_RUN: ${NEO_SKIP_AUB_TESTS_RUN}")

# Set our build directory
if(NOT DEFINED NEO_BUILD_DIR)
  set(NEO_BUILD_DIR ${CMAKE_BINARY_DIR})
endif()

if(NOT NEO_BINARY_DIR)
  set(NEO_BINARY_DIR ${CMAKE_BINARY_DIR})
endif()

# we use c++20
set(CMAKE_CXX_STANDARD 20)
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
if(WIN32)
  if(MSVC)
    string(REPLACE "/MDd" "/MTd" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASEINTERNAL "${CMAKE_CXX_FLAGS_RELEASEINTERNAL}")
    string(REPLACE "/GR" "/GR-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    add_definitions(-D_HAS_STATIC_RTTI=0)
  else()
    string(REPLACE "-D_DLL " "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    string(REPLACE "-D_DLL " "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "-D_DLL " "" CMAKE_CXX_FLAGS_RELEASEINTERNAL "${CMAKE_CXX_FLAGS_RELEASEINTERNAL}")
  endif()
  add_definitions(-DPURGE_DEBUG_KEY_NAMES=1)
else()
  add_definitions(-DPURGE_DEBUG_KEY_NAMES=0)
endif()

if(NOT NEO_SKIP_UNIT_TESTS)
  if(NOT DEFINED GTEST_REPEAT)
    set(GTEST_REPEAT 1)
  endif()
  message(STATUS "GTest repeat count set to ${GTEST_REPEAT}")

  if(NOT DEFINED GTEST_SHUFFLE)
    set(GTEST_SHUFFLE --gtest_shuffle --gtest_random_seed=0)
  endif()
  message(STATUS "GTest shuffle set to ${GTEST_SHUFFLE}")
endif()

get_filename_component(AUB_STREAM_HEADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party${BRANCH_DIR_SUFFIX}aub_stream" ABSOLUTE)
if(IS_DIRECTORY ${AUB_STREAM_HEADERS_DIR})
  message(STATUS "Aub Stream Headers dir: ${AUB_STREAM_HEADERS_DIR}")
else()
  message(FATAL_ERROR "Aub Stream headers not available!")
endif()

if(TARGET aub_stream)
  set(AUB_STREAM_PROJECT_NAME "aub_stream")
  get_target_property(AUB_STREAM_HEADERS_DIR ${AUB_STREAM_PROJECT_NAME} INTERFACE_INCLUDE_DIRECTORIES)
else()
  if(NOT DEFINED AUB_STREAM_DIR)
    get_filename_component(TEMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../aub_stream/aub_mem_dump" ABSOLUTE)
    if(IS_DIRECTORY ${TEMP_DIR})
      get_filename_component(AUB_STREAM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../aub_stream" ABSOLUTE)
      message(STATUS "Aub Stream dir: ${AUB_STREAM_DIR}")
    endif()
  endif()

  if(DISABLE_AUB_STREAM)
    unset(AUB_STREAM_DIR)
  endif()
endif()

if(NOT DEFINED KHRONOS_HEADERS_DIR)
  get_filename_component(DIR_tmp "${CMAKE_CURRENT_SOURCE_DIR}/third_party/opencl_headers" ABSOLUTE)
  if(IS_DIRECTORY ${DIR_tmp})
    set(KHRONOS_HEADERS_DIR ${DIR_tmp})
    add_definitions(-DCL_TARGET_OPENCL_VERSION=300)
  else()
    message(FATAL_ERROR "Khronos OpenCL headers not available!")
  endif()
elseif(DEFINED CL_TARGET_OPENCL_VERSION)
  add_definitions(-DCL_TARGET_OPENCL_VERSION=${CL_TARGET_OPENCL_VERSION})
else()
  add_definitions(-DCL_TARGET_OPENCL_VERSION=300)
endif()
message(STATUS "Khronos OpenCL headers dir: ${KHRONOS_HEADERS_DIR}")
set(OCL_HEADERS_DIR ${KHRONOS_HEADERS_DIR})

if(NOT DEFINED KHRONOS_GL_HEADERS_DIR)
  get_filename_component(GL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/opengl_headers" ABSOLUTE)
  if(IS_DIRECTORY ${GL_DIR})
    set(KHRONOS_GL_HEADERS_DIR ${GL_DIR})
    add_definitions(-DGL_TARGET_OPENGL_VERSION=210)
  endif()
elseif(DEFINED GL_TARGET_OPENGL_VERSION)
  add_definitions(-DGL_TARGET_OPENGL_VERSION=${GL_TARGET_OPENGL_VERSION})
else()
  add_definitions(-DGL_TARGET_OPENGL_VERSION=210)
endif()
message(STATUS "Khronos OpenGL headers dir: ${KHRONOS_GL_HEADERS_DIR}")

if(NOT DEFINED KHRONOS_SPIRV_HEADERS_DIR)
  get_filename_component(SPIRV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/spirv_headers/include" ABSOLUTE)
  if(IS_DIRECTORY ${SPIRV_DIR})
    set(KHRONOS_SPIRV_HEADERS_DIR ${SPIRV_DIR})
  endif()
endif()
message(STATUS "Khronos SPIR-V headers dir: ${KHRONOS_SPIRV_HEADERS_DIR}")

if(NOT DEFINED AOT_CONFIG_HEADERS_DIR)
  get_filename_component(AOT_CONFIG_HEADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party${BRANCH_DIR_SUFFIX}aot_config_headers" ABSOLUTE)
  message(STATUS "AOT config headers dir: ${AOT_CONFIG_HEADERS_DIR}")
endif()

# Intel Graphics Compiler detection
include(cmake${BRANCH_DIR_SUFFIX}/find_igc.cmake)

# GmmLib detection
include(cmake/find_gmmlib.cmake)

# Metrics detection
include(cmake/find_metrics.cmake)

# LibVA detection
if(NOT DISABLE_LIBVA)
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

include(cmake/find_gtest.cmake)

message(STATUS "AUB_STREAM_DIR = ${AUB_STREAM_DIR}")
if(DEFINED AUB_STREAM_DIR)
  set(AUB_STREAM_PROJECT_NAME "aub_stream")
  add_subdirectory_unique(${AUB_STREAM_DIR} ${CMAKE_BINARY_DIR}/aub_stream EXCLUDE_FROM_ALL)

  get_target_property(ENGINE_NODE_DIR ${AUB_STREAM_PROJECT_NAME} INTERFACE_INCLUDE_DIRECTORIES)
  if(NOT EXISTS ${ENGINE_NODE_DIR}/aubstream/engine_node.h)
    set(ENGINE_NODE_DIR ${AUB_STREAM_HEADERS_DIR})
  endif()
else()
  set(ENGINE_NODE_DIR ${AUB_STREAM_HEADERS_DIR})
endif()
message(STATUS "Engine node dir: ${ENGINE_NODE_DIR}")

if(NEO_FORCE_USE_AUB_STREAM AND NOT DEFINED AUB_STREAM_PROJECT_NAME)
  message(FATAL_ERROR "Aubstream is not available but required by user")
endif()

# We want to organize our IDE targets into folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Get available platforms
if(DEFINED NEO_ALLOW_LEGACY_PLATFORMS_SUPPORT AND NOT DEFINED NEO_LEGACY_PLATFORMS_SUPPORT)
  set(NEO_LEGACY_PLATFORMS_SUPPORT ${NEO_ALLOW_LEGACY_PLATFORMS_SUPPORT})
endif()
if(NOT DEFINED NEO_LEGACY_PLATFORMS_SUPPORT)
  set(NEO_LEGACY_PLATFORMS_SUPPORT FALSE)
endif()
if(NOT DEFINED NEO_CURRENT_PLATFORMS_SUPPORT)
  set(NEO_CURRENT_PLATFORMS_SUPPORT TRUE)
endif()
set(NEO__LEGACY_NAME_SUFFIX "_legacy1")
if(NEO_LEGACY_PLATFORMS_SUPPORT AND NOT NEO_CURRENT_PLATFORMS_SUPPORT)
  if(NOT DEFINED NEO_OCLOC_VERSION_MODE)
    set(NEO_OCLOC_VERSION_MODE "0")
  endif()
  set(NEO__SO_NAME_SUFFIX ${NEO__LEGACY_NAME_SUFFIX})
endif()
if(NOT DEFINED NEO_OCLOC_VERSION_MODE)
  set(NEO_OCLOC_VERSION_MODE "1")
endif()
if(NOT DEFINED NEO_BUILD_UNVERSIONED_OCLOC)
  set(NEO_BUILD_UNVERSIONED_OCLOC FALSE)
endif()
include(platforms.cmake)
list(LENGTH ALL_PRODUCT_FAMILY_LIST ALL_PRODUCT_FAMILY_LIST_COUNT)
if(1 EQUAL ${ALL_PRODUCT_FAMILY_LIST_COUNT})
  SET(NEO_SINGLE_SKU_BUILD TRUE)
  message(STATUS "Single Sku Build detected")
endif()

if(WIN32)
  set(DISABLE_WDDM_LINUX TRUE)
endif()

if(PLATFORMS_TO_HAVE_WDDM_DISABLED AND NOT DISABLE_WDDM_LINUX)
  message(STATUS "Platforms to have WDDM_LINUX disabled: ${PLATFORMS_TO_HAVE_WDDM_DISABLED}")
  set(PLATFORMS_TO_HAVE_WDDM_ENABLED ${SUPPORTED_PLATFORM_LIST})
  foreach(SKU_NAME ${PLATFORMS_TO_HAVE_WDDM_DISABLED})
    list(REMOVE_ITEM PLATFORMS_TO_HAVE_WDDM_ENABLED ${SKU_NAME})
  endforeach()
  if(NOT PLATFORMS_TO_HAVE_WDDM_ENABLED)
    set(DISABLE_WDDM_LINUX TRUE)
  endif()
  unset(PLATFORMS_TO_HAVE_WDDM_ENABLED)
endif()
unset(PLATFORMS_TO_HAVE_WDDM_DISABLED)

include(cmake/find_wdk.cmake)

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

if(WIN32 OR NOT DISABLE_WDDM_LINUX)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS -DWDDM_VERSION_NUMBER=23 -DNOMINMAX)
  add_definitions(-DLEGACY_D3DAPI_VERSION=20)
  if(NOT WIN32)
    add_definitions(-DWDDM_LINUX=1)
  endif()
endif()

if(UNIX)
  # i915 prelim detection
  if(NOT ("${BRANCH_TYPE}" STREQUAL ""))
    set(NEO_ENABLE_I915_PRELIM_DETECTION TRUE)
  elseif(NOT DEFINED NEO_ENABLE_I915_PRELIM_DETECTION)
    set(NEO_ENABLE_I915_PRELIM_DETECTION FALSE)
  endif()
  message(STATUS "i915 prelim headers detection: ${NEO_ENABLE_I915_PRELIM_DETECTION}")

  # xe prelim detection
  if(NOT ("${BRANCH_TYPE}" STREQUAL ""))
    set(NEO_ENABLE_XE_PRELIM_DETECTION TRUE)
  elseif(NOT DEFINED NEO_ENABLE_XE_PRELIM_DETECTION)
    set(NEO_ENABLE_XE_PRELIM_DETECTION FALSE)
  endif()
  message(STATUS "xe prelim headers detection: ${NEO_ENABLE_XE_PRELIM_DETECTION}")

  # xe eudebug support
  if(NOT ("${BRANCH_TYPE}" STREQUAL ""))
    set(NEO_ENABLE_XE_EU_DEBUG_SUPPORT TRUE)
  elseif(NOT DEFINED NEO_ENABLE_XE_EU_DEBUG_SUPPORT)
    set(NEO_ENABLE_XE_EU_DEBUG_SUPPORT FALSE)
  endif()
  message(STATUS "Xe EU Debug support: ${NEO_ENABLE_XE_EU_DEBUG_SUPPORT}")

  if(NEO_ENABLE_XE_EU_DEBUG_SUPPORT)
    # xe eudebug exp upstream support
    if(NOT ("${BRANCH_TYPE}" STREQUAL ""))
      set(NEO_USE_XE_EU_DEBUG_EXP_UPSTREAM TRUE)
    elseif(NOT DEFINED NEO_USE_XE_EU_DEBUG_EXP_UPSTREAM)
      set(NEO_USE_XE_EU_DEBUG_EXP_UPSTREAM FALSE)
    endif()
    message(STATUS "Xe EU Debug support for upstream: ${NEO_USE_XE_EU_DEBUG_EXP_UPSTREAM}")
    message(STATUS "Xe EU Debug support for prelim: ${NEO_ENABLE_XE_PRELIM_DETECTION}")
  endif()
endif()

include(cmake${BRANCH_DIR_SUFFIX}/find_drm.cmake)

# Enable/Disable BuiltIns compilation during build
set(COMPILE_BUILT_INS TRUE CACHE BOOL "Enable built-in kernels compilation")
if(NOT DEFINED NEO_KERNELS_BIN_DIR)
  string(REPLACE "internal" "" NEO_KERNELS_BUILD_TYPE_DIR "${BUILD_TYPE_lower}")
  message(STATUS "Neo kernels build type dir: ${NEO_KERNELS_BUILD_TYPE_DIR}")
  if(WIN32)
    get_filename_component(NEO_KERNELS_BIN_DIR "${NEO_SOURCE_DIR}/../kernels_bin/windows/${NEO_KERNELS_BUILD_TYPE_DIR}" ABSOLUTE)
  else()
    get_filename_component(NEO_KERNELS_BIN_DIR "${NEO_SOURCE_DIR}/../kernels_bin/linux" ABSOLUTE)
  endif()
endif()

# Changing the default executable and library output directories
set(IGDRCL_OUTPUT_DIR "${IGDRCL_OPTION__OUTPUT_DIR}")

# do not add rpath
set(CMAKE_SKIP_RPATH YES CACHE BOOL "" FORCE)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

option(APPVERIFIER_ALLOWED "allow use of AppVerifier" OFF)

option(CCACHE_ALLOWED "allow use of ccache" TRUE)
if((CMAKE_HOST_UNIX OR("${CMAKE_GENERATOR}" STREQUAL "Ninja")) AND CCACHE_ALLOWED)
  find_program(CCACHE_EXE_FOUND ccache)
  if(CCACHE_EXE_FOUND)
    message(STATUS "Found ccache: ${CCACHE_EXE_FOUND}")

    if(NOT NEO_DONT_SET_CCACHE_BASEDIR AND NOT DEFINED ENV{CCACHE_BASEDIR})
      get_filename_component(__tmp_dir "../infra" ABSOLUTE)
      if(IS_DIRECTORY ${__tmp_dir})
        get_filename_component(NEO_CCACHE_BASEDIR ".." ABSOLUTE)
        if(CMAKE_HOST_WIN32)
          list(APPEND NEO_RULE_LAUNCH_LIST "\"${CMAKE_COMMAND}\" -E env CCACHE_BASEDIR=${NEO_CCACHE_BASEDIR}")
        else()
          list(APPEND NEO_RULE_LAUNCH_LIST "CCACHE_BASEDIR=${NEO_CCACHE_BASEDIR}")
        endif()

        message(STATUS "setting ccache base dir: ${NEO_CCACHE_BASEDIR}")
      endif()
    elseif(DEFINED ENV{CCACHE_BASEDIR})
      message(STATUS "Using ccache base dir: $ENV{CCACHE_BASEDIR}")
    endif()
    list(APPEND NEO_RULE_LAUNCH_LIST "${CCACHE_EXE_FOUND}")
    string(REPLACE ";" " " NEO_RULE_LAUNCH_STR "${NEO_RULE_LAUNCH_LIST}")
    message(STATUS "Using rule: ${NEO_RULE_LAUNCH_STR}")

    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${NEO_RULE_LAUNCH_STR})
    if("${CMAKE_GENERATOR}" STREQUAL "Ninja")
      set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ${NEO_RULE_LAUNCH_STR})
    else()
      set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CCACHE_EXE_FOUND}")
    endif()
  endif()
endif()

# Miscs options
option(NEO_ULTS_ENABLE_OPTIMIZATIONS "Enable compiler optimizations in ULTs builds" OFF)

option(IGDRCL_GCOV "generate gcov report" OFF)

option(NEO_SHOW_VERBOSE_ULT_RESULTS "Use the default/verbose test output" OFF)

if(NOT NEO_SHOW_VERBOSE_ULT_RESULTS)
  set(NEO_TESTS_LISTENER_OPTION "--disable_default_listener")
else()
  set(NEO_TESTS_LISTENER_OPTION "--enable_default_listener")
endif()

if(MSVC)
  # Force to treat warnings as errors
  if(NOT CMAKE_CXX_FLAGS MATCHES "/WX")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX")
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /we4189 /we4388")
    endif()
  endif()

  # Disable generating manifest
  set(LINKER_FLAGS "/MANIFEST:NO")

  # Disable COMDAT folding optimization
  set(LINKER_FLAGS "${LINKER_FLAGS} /OPT:NOICF")

  # Use legacy float rounding
  set(LINKER_FLAGS "${LINKER_FLAGS} legacy_stdio_float_rounding.obj")

  set(LINKER_FLAGS "${LINKER_FLAGS} /CETCOMPAT")

  # Support for WUD
  set(CMAKE_CXX_STANDARD_LIBRARIES "onecore.lib")
  foreach(IT kernel32.lib;user32.lib;gdi32.lib;advapi32.lib;ole32.lib;)
    set(LINKER_FLAGS "${LINKER_FLAGS} /NODEFAULTLIB:${IT}")
  endforeach()
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LINKER_FLAGS}")
  set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${LINKER_FLAGS}")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${LINKER_FLAGS}")
  add_definitions(-DUNICODE -D_UNICODE)
else()
  if(IGDRCL_GCOV)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage --coverage")
  endif()
  option(USE_ASAN "Link with address sanitization support" OFF)
  if(USE_ASAN)
    if(CMAKE_COMPILER_IS_GNUCC)
      set(ASAN_FLAGS " -fsanitize=address -fno-omit-frame-pointer")
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
      set(TSAN_FLAGS " -fsanitize=thread")
      set(TSAN_LIBS "tsan")
    endif()
  endif()

endif()

# increase the maximum number of sections in .obj files
if(MSVC)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /bigobj")
  set(CMAKE_CXX_FLAGS_RELEASEINTERNAL "${CMAKE_CXX_FLAGS_RELEASEINTERNAL} /bigobj")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /bigobj")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")
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
elseif(WIN32 AND "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" AND NOT NO_PDB)
  add_compile_options(-gcodeview)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g")
  set(CMAKE_CXX_FLAGS_RELEASEINTERNAL "${CMAKE_CXX_FLAGS_RELEASEINTERNAL} -g")
endif()

# spectre mitigation
if(NOT NEO_DISABLE_MITIGATIONS)
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
  endif()
else()
  message(WARNING "Spectre mitigation DISABLED")
endif()

if(NOT MSVC)
  check_cxx_compiler_flag(-msse4.2 COMPILER_SUPPORTS_SSE42)
  check_cxx_compiler_flag(-mavx2 COMPILER_SUPPORTS_AVX2)
  check_cxx_compiler_flag(-march=armv8-a+simd COMPILER_SUPPORTS_NEON)
endif()

if(NOT MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-depth=1024")
endif()

# intrinsics (_mm_clflushopt and waitpkg) support
if(NOT MSVC)
  check_cxx_compiler_flag(-mclflushopt SUPPORTS_CLFLUSHOPT)
  check_cxx_compiler_flag(-mwaitpkg SUPPORTS_WAITPKG)
  if(SUPPORTS_CLFLUSHOPT)
    add_compile_definitions(SUPPORTS_CLFLUSHOPT)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mclflushopt")
  endif()
  if(SUPPORTS_WAITPKG)
    add_compile_definitions(SUPPORTS_WAITPKG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mwaitpkg")
  else()
    message(WARNING "-mwaitpkg flag is not supported by the compiler")
  endif()
else()
  add_compile_definitions(SUPPORTS_CLFLUSHOPT)
  add_compile_definitions(SUPPORTS_WAITPKG)
endif()

# Compiler warning flags
if(NOT MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Winvalid-pch")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wformat -Wformat-security")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wvla")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-pragmas") # needed for pragmas in case of MSVC
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter") # needed for unused function arguments
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-field-initializers") # needed for dependencies, e.g. gmmlib

  if("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
    # clang only
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wshorten-64-to-32 -Wno-extern-c-compat -Wno-instantiation-after-specialization")
  else()
    # gcc only
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 12)
      # Disable specific warnings due to GCC bug for versions >= 12
      # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=111073
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-stringop-overflow")
    endif()
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
      # Disable specific warnings due to GCC bug for version 12
      if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 12 AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 13)
        # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105329
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-restrict")
      endif()
      # Disable specific warnings due to GCC bug for versions >= 12
      if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 12)
        # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105616
        # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105562
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-maybe-uninitialized")
      endif()
    endif()
  endif()

  if(USE_SANITIZE_UB)
    check_cxx_compiler_flag(-fsanitize=undefined COMPILER_SUPPORTS_UNDEFINED_BEHAVIOR_SANITIZER)
    if(COMPILER_SUPPORTS_UNDEFINED_BEHAVIOR_SANITIZER)
      message(STATUS "Enabling undefined behavior sanitizer")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fsanitize-recover=vptr -fno-rtti")
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
    string(FIND ${CMAKE_CXX_FLAGS} "-D_FORTIFY_SOURCE=3" __FORTIFY_SOURCE_3_SET)
    if(${__FORTIFY_SOURCE_3_SET} LESS 0)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -D_FORTIFY_SOURCE=2")
    endif()

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-strong")
    if(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    else()
      # gcc, g++ only
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
include_directories(${NEO_SHARED_DIRECTORY}/command_container/definitions${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SHARED_DIRECTORY}/command_stream/definitions${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SHARED_DIRECTORY}/debug_settings/definitions${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SHARED_DIRECTORY}/dll/devices${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SHARED_DIRECTORY}/kernel/definitions${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SHARED_DIRECTORY}/gen_common${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SHARED_DIRECTORY}/gmm_helper/${DRIVER_MODEL})
if(WIN32)
  include_directories(${NEO_SHARED_DIRECTORY}/gmm_helper/windows/gmm_memory${BRANCH_DIR_SUFFIX})
  include_directories(${NEO_SHARED_DIRECTORY}/os_interface/windows/wddm/definitions)
else()
  include_directories(${NEO_LINUX_KMD_HEADERS_DIR}
                      ${NEO_SHARED_DIRECTORY}/gmm_helper/windows/gmm_memory
  )
  if("${BRANCH_TYPE}" STREQUAL "")
    include_directories(${NEO_SOURCE_DIR}/third_party/uapi/prelim)
  endif()
  if(NOT DISABLE_WDDM_LINUX)
    include_directories(${NEO_SHARED_DIRECTORY}/os_interface/linux/wddm_linux/definitions)
  endif()
endif()
include_directories(${NEO_SHARED_DIRECTORY}/helpers/definitions${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SHARED_DIRECTORY}/memory_properties${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SHARED_DIRECTORY}/release_helper/definitions${BRANCH_DIR_SUFFIX})
include_directories(${NEO_SHARED_DIRECTORY}/sku_info/definitions${BRANCH_DIR_SUFFIX})

macro(macro_for_each_platform)
  string(TOLOWER ${PLATFORM_IT} PLATFORM_IT_LOWER)
  foreach(PLATFORM_DIRECTORY
            ${NEO_SHARED_DIRECTORY}/${CORE_TYPE_LOWER}/${PLATFORM_IT_LOWER}/definitions${BRANCH_DIR_SUFFIX}
            ${NEO_SHARED_DIRECTORY}/ail/${CORE_TYPE_LOWER}/${PLATFORM_IT_LOWER}/definitions${BRANCH_DIR_SUFFIX}
    )
    if(EXISTS ${PLATFORM_DIRECTORY})
      include_directories(${PLATFORM_DIRECTORY})
    endif()
  endforeach()
endmacro()

macro(macro_for_each_core_type)
  foreach(PLATFORM_DIRECTORY ${NEO_SHARED_DIRECTORY}/${CORE_TYPE_LOWER}/definitions${BRANCH_DIR_SUFFIX} ${NEO_SOURCE_DIR}/opencl/source/${CORE_TYPE_LOWER}/definitions${BRANCH_DIR_SUFFIX})
    if(EXISTS ${PLATFORM_DIRECTORY})
      include_directories(${PLATFORM_DIRECTORY})
    endif()
  endforeach()

  apply_macro_for_each_platform("SUPPORTED")
endmacro()

apply_macro_for_each_core_type("SUPPORTED")

# Define where to put binaries
set(TargetDir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
if(MSVC)
  if(NOT "${CMAKE_GENERATOR}" STREQUAL "Ninja")
    set(TargetDir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE})
  endif()
endif()

add_subdirectory_unique(shared/source/release_helper)
add_subdirectory_unique(${OCLOC_DIRECTORY} ${NEO_BUILD_DIR}/offline_compiler)
target_compile_definitions(ocloc_lib
                           PRIVATE MOCKABLE_VIRTUAL=
)
include(cmake/ocloc_cmd_prefix.cmake)

if(DONT_CARE_OF_VIRTUALS)
  set(NEO_SHARED_RELEASE_LIB_NAME "neo_shared")
  set(NEO_SHARED_MOCKABLE_LIB_NAME ${NEO_SHARED_RELEASE_LIB_NAME})
else()
  set(NEO_SHARED_RELEASE_LIB_NAME "neo_shared")
  if(NOT NEO_SKIP_UNIT_TESTS)
    set(NEO_SHARED_MOCKABLE_LIB_NAME "neo_shared_mockable")
  endif()
endif()

set(BIKSIM_LIB_NAME "biksim")
set(BUILTINS_SOURCES_LIB_NAME "builtins_sources")
set(BUILTINS_BINARIES_STATELESS_LIB_NAME "builtins_binaries_stateless")
set(BUILTINS_BINARIES_HEAPLESS_LIB_NAME "builtins_binaries_heapless")
set(BUILTINS_BINARIES_BINDFUL_LIB_NAME "builtins_binaries_bindful")
set(BUILTINS_BINARIES_BINDLESS_LIB_NAME "builtins_binaries_bindless")
set(BUILTINS_SPIRV_LIB_NAME "builtins_spirv")

if(WIN32)
  set(NEO_EXTRA_LIBS Ws2_32 winmm)
else()
  set(NEO_EXTRA_LIBS dl pthread rt)
endif()

add_subdirectory_unique(shared)

if(NEO_BUILD_WITH_OCL)
  add_subdirectory_unique(opencl)
else()
  hide_subdir(opencl)
  set(NEO_SKIP_OCL_UNIT_TESTS TRUE)
endif()

set(DONT_LINK_ELF_STATICALLY TRUE)
if(EXISTS ${NEO_SOURCE_DIR}/../internal)
  add_subdirectory_unique(${NEO_SOURCE_DIR}/../internal ${NEO_BUILD_DIR}/internal)
endif()

set(NEO_SCRIPT_PROJECTS_FOLDER "neo scripts")

set(NEO_SHARED_BUILTINS_DIR "${NEO_SOURCE_DIR}/shared/source/built_ins/kernels")
set(NEO_SHARED_TEST_FILES_DIR "${NEO_SOURCE_DIR}/shared/test/common/test_files")
set(NEO_OPENCL_TEST_FILES_DIR "${NEO_SOURCE_DIR}/opencl/test/unit_test/test_files")

configure_file(config.h.in ${NEO_BUILD_DIR}/config.h)
configure_file(driver_version.h.in ${NEO_BUILD_DIR}/driver_version.h) # Put Driver version into define
configure_file(test_files_setup.h.in ${NEO_BUILD_DIR}/test_files_setup.h)
configure_file(hw_cmds_default.h.in ${NEO_BUILD_DIR}/hw_cmds_default.h)

if(BUILD_WITH_L0)
  add_subdirectory_unique(level_zero)
endif()

add_subdirectory_unique(target_unit_tests)
add_subdirectory_unique(target_aub_tests)

add_subdirectories()

include(package.cmake)
