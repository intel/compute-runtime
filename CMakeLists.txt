# Copyright (c) 2017 - 2018, Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.

cmake_minimum_required(VERSION 3.2.0 FATAL_ERROR)
include(ExternalProject)
# Include needed macros
include(common_macros.cmake)
# Set the configuration type
set(CMAKE_CONFIGURATION_TYPES
	${CMAKE_BUILD_TYPE}
)

project(igdrcl)

if(TR_DEPRECATED)
    add_definitions(-D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING=1)
endif(TR_DEPRECATED)

if(NOT CMAKE_BUILD_TYPE)
	message(STATUS "CMAKE_BUILD_TYPE not specified, using Release")
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type: [Release, ReleaseInternal, Debug]" FORCE)
endif()

set(CMAKE_C_FLAGS_RELEASEINTERNAL ${CMAKE_C_FLAGS_RELEASE})
set(CMAKE_CXX_FLAGS_RELEASEINTERNAL ${CMAKE_CXX_FLAGS_RELEASE})
set(CMAKE_SHARED_LINKER_FLAGS_RELEASEINTERNAL ${CMAKE_SHARED_LINKER_FLAGS_RELEASE})
set(CMAKE_EXE_LINKER_FLAGS_RELEASEINTERNAL ${CMAKE_EXE_LINKER_FLAGS_RELEASE})

string(TOLOWER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_lower)
if("${BUILD_TYPE_lower}" STREQUAL "releaseinternal")
	add_definitions(-D_RELEASE_INTERNAL)
endif("${BUILD_TYPE_lower}" STREQUAL "releaseinternal")


message(STATUS "${CMAKE_BUILD_TYPE} build configuration")

# Set the runtime source directory
if(NOT DEFINED IGDRCL_SOURCE_DIR)
	set(IGDRCL_SOURCE_DIR ${CMAKE_SOURCE_DIR})
endif()

# Set our build directory
if(NOT DEFINED IGDRCL_BUILD_DIR)
	set(IGDRCL_BUILD_DIR ${CMAKE_BINARY_DIR})
endif()

if(NOT IGDRCL_BINARY_DIR)
	set(IGDRCL_BINARY_DIR ${CMAKE_BINARY_DIR})
endif()

# we use c++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# we force using response files
set(CMAKE_NINJA_FORCE_RESPONSE_FILE 1)

set(COMMON_CLANG_LIBRARY_NAME "opencl_clang")

# We want to build with the static, multithreaded runtime libraries (as opposed
# to the multithreaded runtime DLLs)
if(MSVC)
	string(REPLACE "/MDd" "/MTd" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
	string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
	string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASEINTERNAL "${CMAKE_CXX_FLAGS_RELEASEINTERNAL}")
endif(MSVC)

if(NOT GTEST_SRC_DIR)
	set(GTEST_SRC_DIR_tmp "${CMAKE_SOURCE_DIR}/../gtest")
	get_filename_component(GTEST_SRC_DIR ${GTEST_SRC_DIR_tmp} ABSOLUTE)
	set(GMOCK_SRC_DIR_tmp "${CMAKE_SOURCE_DIR}/../gmock")
	get_filename_component(GMOCK_SRC_DIR ${GMOCK_SRC_DIR_tmp} ABSOLUTE)
else(NOT GTEST_SRC_DIR)
	get_filename_component(GTEST_SRC_DIR ${GTEST_SRC_DIR} ABSOLUTE)
	set(GMOCK_SRC_DIR_tmp "${GTEST_SRC_DIR}/../gmock")
	get_filename_component(GMOCK_SRC_DIR ${GMOCK_SRC_DIR_tmp} ABSOLUTE)
endif(NOT GTEST_SRC_DIR)
set(GTEST_INCLUDE_DIR "${GTEST_SRC_DIR}/include")
set(GMOCK_INCLUDE_DIR "${GMOCK_SRC_DIR}/include")
message(STATUS "Google Test source dir: ${GTEST_SRC_DIR}")
message(STATUS "Google Mock source dir: ${GMOCK_SRC_DIR}")
add_subdirectory(${GMOCK_SRC_DIR} ${IGDRCL_BINARY_DIR}/gmock)
set_target_properties(gtest PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
set_target_properties(gmock PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(NEO_BITS "64")
	set(NEO_ARCH "x64")
else()
	set(NEO_BITS "32")
	set(NEO_ARCH "x86")
endif()

if(NOT ARTIFACTS_DIR)
	get_filename_component(ARTIFACTS_DIR_tmp "${CMAKE_SOURCE_DIR}/../artifacts" ABSOLUTE)
	if(IS_DIRECTORY "${ARTIFACTS_DIR_tmp}")
		set(ARTIFACTS_DIR "${ARTIFACTS_DIR_tmp}")
	endif()
endif(NOT ARTIFACTS_DIR)
if(ARTIFACTS_DIR)
	message(STATUS "Artifact directory is ${ARTIFACTS_DIR}")
endif(ARTIFACTS_DIR)

if(NOT GTPIN_HEADERS_DIR)
    if ((EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../internal/gtpin/gtpin_ocl_interface.h") AND (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../internal/gtpin/gtpin_driver_common.h"))
	    get_filename_component(GTPIN_HEADERS_DIR "../internal/gtpin/" ABSOLUTE)
        message(STATUS "GT-Pin headers dir: ${GTPIN_HEADERS_DIR}")
    endif()
endif(NOT GTPIN_HEADERS_DIR)

if(NOT KMDAF_HEADERS_DIR)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../kmdaf/inc/common/kmDaf.h")
        get_filename_component(KMDAF_HEADERS_DIR "../kmdaf/inc/common/" ABSOLUTE)
        message(STATUS "KM-DAF headers dir: ${KMDAF_HEADERS_DIR}")
    endif()
endif(NOT KMDAF_HEADERS_DIR)

if(NOT LIBDRM_DIR)
	get_filename_component(LIBDRM_DIR "../libdrm/" ABSOLUTE)
endif(NOT LIBDRM_DIR)
message(STATUS "libdrm dir: ${LIBDRM_DIR}")

if(NOT KHRONOS_HEADERS_DIR)
	get_filename_component(KHRONOS_HEADERS_DIR "../khronos/opencl21/" ABSOLUTE)
endif(NOT KHRONOS_HEADERS_DIR)
message(STATUS "Khronos OpenCL headers dir: ${KHRONOS_HEADERS_DIR}")
set(OCL_HEADERS_DIR ${KHRONOS_HEADERS_DIR})

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

	set(WDK_INCLUDE_PATHS
		"${WDK_DIR}/Include/${WindowsTargetPlatformVersion}/um"
		"${WDK_DIR}/Include/${WindowsTargetPlatformVersion}/shared"
		"${WDK_DIR}/Include/${WindowsTargetPlatformVersion}/km"
	)
	message(STATUS "WDK include paths: ${WDK_INCLUDE_PATHS}")
endif()

# GmmLib detection
if(NOT GMMUMD_LIB_NAME)
    if(TARGET gmm_umd)
        set(GMMUMD_LIB_NAME "gmm_umd")
    else()
        if(GMM_SOURCE_DIR)
            get_filename_component(GMM_SOURCE_DIR "${GMM_SOURCE_DIR}" ABSOLUTE)
        else(GMM_SOURCE_DIR)
            get_filename_component(GMM_SOURCE_DIR_tmp "${CMAKE_SOURCE_DIR}/../gmmlib" ABSOLUTE)
            if(IS_DIRECTORY "${GMM_SOURCE_DIR_tmp}")
                set(GMM_SOURCE_DIR "${GMM_SOURCE_DIR_tmp}")
            endif()
        endif()
        if(NOT IS_DIRECTORY "${GMM_SOURCE_DIR}")
            message(FATAL_ERROR "GmmLib not found!")
        endif()

        message(STATUS "GmmLib source dir is: ${GMM_SOURCE_DIR}")
        add_subdirectory("${GMM_SOURCE_DIR}" "${IGDRCL_BUILD_DIR}/gmmlib")

        if(NOT GMMUMD_LIB_NAME)
            set(GMMUMD_LIB_NAME "gmm_umd")
        endif()
    endif()
endif()
set(UMKM_SHAREDDATA_INCLUDE_PATHS $<TARGET_PROPERTY:${GMMUMD_LIB_NAME},INTERFACE_INCLUDE_DIRECTORIES>)

# Intel Graphics Compiler detection
if(NOT IGC__IGC_TARGETS)
    if(IGC_DIR)
        get_filename_component(IGC_DIR "${IGC_DIR}" ABSOLUTE)
    else(IGC_DIR)
        get_filename_component(IGC_DIR_tmp "${CMAKE_SOURCE_DIR}/../igc" ABSOLUTE)
        if(IS_DIRECTORY "${IGC_DIR_tmp}")
            set(IGC_DIR "${IGC_DIR_tmp}")
        endif()
    endif()
    if(NOT IS_DIRECTORY "${IGC_DIR}")
        message(FATAL_ERROR "Intel Graphics Compiler not found!")
    endif()

    message(STATUS "IGC source dir is: ${IGC_DIR}")

    set(IGC_OPTION__LIBRARY_NAME "igdccl")
    set(IGC_OPTION__OUTPUT_DIR "${IGDRCL_BUILD_DIR}/igc")
    set(IGC_OPTION__INCLUDE_IGC_COMPILER_TOOLS OFF)
    add_subdirectory("${IGC_DIR}" "${IGDRCL_BUILD_DIR}/igc" EXCLUDE_FROM_ALL)
endif()

set(IGDRCL__IGC_TARGETS "${IGC__IGC_TARGETS}")
foreach(TARGET_tmp ${IGDRCL__IGC_TARGETS})
    list(APPEND IGDRCL__IGC_INCLUDE_DIR $<TARGET_PROPERTY:${TARGET_tmp},INTERFACE_INCLUDE_DIRECTORIES>)
endforeach(TARGET_tmp)
message(STATUS "IGC targets: ${IGDRCL__IGC_TARGETS}")

# Instrumentation detection
if(NOT INSTRUMENTATION_LIB_NAME)
    if(INSTRUMENTATION_SOURCE_DIR)
        get_filename_component(INSTRUMENTATION_SOURCE_DIR "${INSTRUMENTATION_SOURCE_DIR}" ABSOLUTE)
    else(INSTRUMENTATION_SOURCE_DIR)
        get_filename_component(INSTRUMENTATION_SOURCE_DIR_tmp "${CMAKE_SOURCE_DIR}/../instrumentation" ABSOLUTE)
        if(IS_DIRECTORY "${INSTRUMENTATION_SOURCE_DIR_tmp}")
            set(INSTRUMENTATION_SOURCE_DIR "${INSTRUMENTATION_SOURCE_DIR_tmp}")
        endif()
    endif()
    if(IS_DIRECTORY "${INSTRUMENTATION_SOURCE_DIR}")
        message(STATUS "Intrumentation source dir is: ${INSTRUMENTATION_SOURCE_DIR}")
        add_subdirectory("${INSTRUMENTATION_SOURCE_DIR}" "${IGDRCL_BUILD_DIR}/instrumentation")
        set(INSTRUMENTATION_LIB_NAME "instrumentation_umd")
        set(HAVE_INSTRUMENTATION TRUE)
    endif()
else()
    set(HAVE_INSTRUMENTATION TRUE)
    message(STATUS "Instrumentation library name: ${INSTRUMENTATION_LIB_NAME}")
endif()

add_definitions(-DGMM_OCL)

# We want to organize our IDE targets into folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Get available platfroms
include(platforms.cmake)

# Enable/Disable BuiltIns compilatoin during build
set(COMPILE_BUILT_INS TRUE CACHE BOOL "Enable built-in kernels compilation")

# Changing the default executable and library output directories
set(IGDRCL_OUTPUT_DIR "${IGDRCL_OPTION__OUTPUT_DIR}")

# set output paths
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${IGDRCL_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${IGDRCL_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${IGDRCL_BINARY_DIR}/lib)

# do not add rpath
set(CMAKE_SKIP_RPATH YES CACHE BOOL "" FORCE )

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

option(APPVERIFIER_ALLOWED "allow use of AppVerifier" TRUE)

option(CCACHE_ALLOWED "allow use of ccache" TRUE)
find_program(CCACHE_EXE_FOUND ccache)
if(CCACHE_EXE_FOUND AND CCACHE_ALLOWED)
	message(STATUS "Found ccache: ${CCACHE_EXE_FOUND}")
	set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
	set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif()

if(UNIX)
	find_package(PkgConfig)
	pkg_check_modules(LIBVA QUIET libva)
	if(LIBVA_FOUND)
		add_definitions(-DLIBVA)
		message(STATUS "Using libva")
	endif()
endif()

# Miscs options
option(IGDRCL_GCOV "generate gcov report" OFF)
option(HAVE_TBX_SERVER "Compile TBX server from TbxAccess library" OFF)
option(USE_CL_CACHE "Use OpenCL program binary cache" ON)
set(CL_CACHE_LOCATION "cl_cache" CACHE STRING "OpenCL program binary cache location")

if(NOT NEO_DRIVER_VERSION)
  set(NEO_DRIVER_VERSION 1.0)
endif()

# Put profiling enable flag into define
if(OCL_RUNTIME_PROFILING)
	add_definitions(-DOCL_RUNTIME_PROFILING=${OCL_RUNTIME_PROFILING})
endif()

if(MSVC)
	# Force to treat warnings as errors
	if(NOT CMAKE_CXX_FLAGS MATCHES "/WX")
		 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX")
	endif()

# Support for WUD
	set(CMAKE_CXX_STANDARD_LIBRARIES "onecore.lib")
	set(LINKER_FLAGS "")
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
			set(ASAN_FLAGS " -fsanitize=address -fno-omit-frame-pointer")
			link_libraries(asan)
		else()
			message(STATUS "Address sanitization with clang not yet support")
		endif()
	endif()
	if(USE_TSAN)
		if(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
			set(TSAN_FLAGS " -fsanitize=thread")
			link_libraries(tsan)
		else()
			message(STATUS "Thread sanitization with gcc is not fully supported")
		endif()
	endif()

	include(CheckLibraryExists)
	CHECK_LIBRARY_EXISTS(rt clock_gettime "time.h" HAVE_CLOCK_GETTIME)
	if(HAVE_CLOCK_GETTIME)
		link_libraries(rt)
	endif(HAVE_CLOCK_GETTIME)
endif(MSVC)

# setup variables needed for custom configuration type
# generate PDB files even for release build on MSVC
if(MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
	set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")

	set(CMAKE_CXX_FLAGS_RELEASEINTERNAL "${CMAKE_CXX_FLAGS_RELEASEINTERNAL} /Zi")
	set(CMAKE_SHARED_LINKER_FLAGS_RELEASEINTERNAL "${CMAKE_SHARED_LINKER_FLAGS_RELEASEINTERNAL} /DEBUG /OPT:REF /OPT:ICF")
endif()

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

	if("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang" )
		# clang only
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wshorten-64-to-32")
		if(USE_SANITIZE_UB)
			message(STATUS "Enabling undefined behavior sanitizer")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fno-sanitize-recover=undefined -fsanitize-recover=vptr -fno-rtti")
		endif(USE_SANITIZE_UB)
		if (NOT (CMAKE_C_COMPILER_VERSION VERSION_LESS 3.6))
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-local-typedefs")
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
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-noexcept-type")	# Added for gtest
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
		if("${NEO_ARCH}" STREQUAL "x32")
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
set(BRANCH_DIR_SUFFIX "/")
include_directories(${IGDRCL_SOURCE_DIR})
include_directories(${IGDRCL_BUILD_DIR})
include_directories(${IGDRCL_SOURCE_DIR}/runtime/sku_info/definitions${BRANCH_DIR_SUFFIX})
include_directories(${IGDRCL_SOURCE_DIR}/runtime/gmm_helper/${BRANCH_DIR_SUFFIX})

set(HW_SRC_INCLUDE_PATH ${IGDRCL_SOURCE_DIR}/runtime/gen_common)

if(HAVE_INSTRUMENTATION)
  set(IGDRCL__INSTRUMENTATION_DIR_SUFFIX ${BRANCH_DIR_SUFFIX})
  include_directories($<TARGET_PROPERTY:${INSTRUMENTATION_LIB_NAME},INTERFACE_INCLUDE_DIRECTORIES>)
else()
  set(IGDRCL__INSTRUMENTATION_DIR_SUFFIX "/")
endif()
include_directories(${IGDRCL_SOURCE_DIR}/runtime/instrumentation${IGDRCL__INSTRUMENTATION_DIR_SUFFIX})

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

add_subdirectory(offline_compiler ${IGDRCL_BUILD_DIR}/offline_compiler)
target_compile_definitions(cloc PUBLIC MOCKABLE_VIRTUAL=)

macro(generate_runtime_lib LIB_NAME MOCKABLE GENERATE_EXEC)
	set(NEO_STATIC_LIB_NAME ${LIB_NAME})
	set(SHARINGS_ENABLE_LIB_NAME "${LIB_NAME}_sharings_enable")
	set(GENERATE_EXECUTABLE ${GENERATE_EXEC})

	add_subdirectory(runtime "${IGDRCL_BUILD_DIR}/${LIB_NAME}")
	target_compile_definitions(${BUILTINS_SOURCES_LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=)
	target_compile_definitions(${BUILTINS_BINARIES_LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=)
	target_compile_definitions(${SCHEDULER_BINARY_LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=)


	if(${MOCKABLE})
		target_compile_definitions(${LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=virtual)
		target_compile_definitions(${SHARINGS_ENABLE_LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=virtual)
		target_compile_definitions(${LIB_NAME} PUBLIC DEFAULT_TEST_PLATFORM=${DEFAULT_TESTED_PLATFORM} DEFAULT_TEST_FAMILY_NAME=${DEFAULT_TESTED_FAMILY_NAME})
	else()
		target_compile_definitions(${LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=)
		target_compile_definitions(${SHARINGS_ENABLE_LIB_NAME} PUBLIC MOCKABLE_VIRTUAL=)
	endif()
endmacro(generate_runtime_lib)

set(NEO_MOCKABLE_LIB_NAME "igdrcl_lib_mockable") # Used by ULTS
set(NEO_RELEASE_LIB_NAME "igdrcl_lib_release") # Used by dll/so
set(NEO_DYNAMIC_LIB_NAME "igdrcl_dll") # single NEO dll
set(NEO_DLL_NAME_BASE "igdrcl")

set(BIKSIM_LIB_NAME "biksim")
set(BUILTINS_SOURCES_LIB_NAME "builtins_sources")
set(BUILTINS_BINARIES_LIB_NAME "builtins_binaries")
set(SCHEDULER_BINARY_LIB_NAME "scheduler_binary")

add_subdirectory(elf)
generate_runtime_lib(${NEO_RELEASE_LIB_NAME} FALSE TRUE)
generate_runtime_lib(${NEO_MOCKABLE_LIB_NAME} TRUE FALSE)

if(EXISTS ../icd)
	add_subdirectory(../icd ${IGDRCL_BINARY_DIR}/icd)
endif(EXISTS ../icd)

if(TARGET OpenCL)
	target_include_directories(OpenCL PRIVATE ${KHRONOS_HEADERS_DIR})
endif()

if(DEFAULT_TESTED_PLATFORM)
  add_subdirectory(unit_tests ${IGDRCL_BUILD_DIR}/unit_tests)
endif()

set(DONT_LINK_ELF_STATICALLY TRUE)
if(EXISTS ${IGDRCL_SOURCE_DIR}/../internal)
	add_subdirectory(${IGDRCL_SOURCE_DIR}/../internal ${IGDRCL_BUILD_DIR}/internal)
endif(EXISTS ${IGDRCL_SOURCE_DIR}/../internal)

set(CL_USE_DEPRECATED_OPENCL_1_1_APIS 1)
set(CL_USE_DEPRECATED_OPENCL_1_2_APIS 1)
set(CL_USE_DEPRECATED_OPENCL_2_0_APIS 1)
set(_CRT_SECURE_NO_WARNINGS 1)

include(package.cmake)

configure_file(config.h.in ${IGDRCL_BUILD_DIR}/config.h)
