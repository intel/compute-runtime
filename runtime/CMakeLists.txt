# Copyright (c) 2018, Intel Corporation
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

if (POLICY CMP0042)
  cmake_policy (SET CMP0042 NEW)
endif (POLICY CMP0042)

if (POLICY CMP0063)
  cmake_policy (SET CMP0063 NEW)
endif (POLICY CMP0063)

project (neo)

#set (CMAKE_CXX_VISIBILITY_PRESET default)
#set (CMAKE_VISIBILITY_INLINES_HIDDEN 1)

# Support for Windows Universal Drivers
ENABLE_WUD()

if(NOT (TARGET ${BIKSIM_LIB_NAME}))
  add_subdirectory(builtin_kernels_simulation)
endif(NOT (TARGET ${BIKSIM_LIB_NAME}))

add_subdirectory(accelerators)
add_subdirectory(api)
add_subdirectory(aub_mem_dump)
add_subdirectory(built_ins)
add_subdirectory(command_queue)
add_subdirectory(command_stream)
add_subdirectory(commands)
add_subdirectory(compiler_interface)
add_subdirectory(context)
add_subdirectory(device)
add_subdirectory(device_queue)
add_subdirectory(event)
add_subdirectory(execution_model)
add_subdirectory(gen_common)
add_subdirectory(gmm_helper)

set(RUNTIME_SRCS_GMM_HELPER ${RUNTIME_SRCS_GMM_HELPER_BASE})
if(WIN32)
  list(APPEND RUNTIME_SRCS_GMM_HELPER ${RUNTIME_SRCS_GMM_HELPER_WINDOWS})
endif()

add_subdirectory(gtpin)
add_subdirectory(helpers)

set(RUNTIME_SRCS_HELPERS ${RUNTIME_SRCS_HELPERS_BASE})
if(WIN32)
  list(APPEND RUNTIME_SRCS_HELPERS ${RUNTIME_SRCS_HELPERS_WINDOWS})
endif()

add_subdirectory(indirect_heap)
add_subdirectory(instrumentation${IGDRCL__INSTRUMENTATION_DIR_SUFFIX})
add_subdirectory(kernel)
add_subdirectory(mem_obj)
add_subdirectory(memory_manager)

set (RUNTIME_SRCS_OS_INTERFACE
  os_interface/32bit_memory.h
  os_interface/os_library.h
  os_interface/device_factory.h
  os_interface/os_inc_base.h
  os_interface/os_interface.h
  os_interface/os_time.h
  os_interface/os_time.cpp
  os_interface/debug_settings_manager.cpp
  os_interface/debug_settings_manager.h
  os_interface/performance_counters.cpp
  os_interface/performance_counters.h
  os_interface/print.h
)

add_subdirectory(platform)
add_subdirectory(program)
add_subdirectory(sampler)
add_subdirectory(scheduler)
add_subdirectory(sharings)
add_subdirectory(sku_info)

set(RUNTIME_SRCS_SKU_INFO ${RUNTIME_SRCS_SKU_INFO_BASE})
if(WIN32)
  list(APPEND RUNTIME_SRCS_SKU_INFO ${RUNTIME_SRCS_SKU_INFO_WINDOWS})
endif()

add_subdirectory(tbx)
add_subdirectory(utilities)

set(RUNTIME_SRCS_UTILITIES ${RUNTIME_SRCS_UTILITIES_BASE})
if(WIN32)
  list(APPEND RUNTIME_SRCS_UTILITIES ${RUNTIME_SRCS_UTILITIES_WINDOWS})
else()
  list(APPEND RUNTIME_SRCS_UTILITIES ${RUNTIME_SRCS_UTILITIES_LINUX})
endif()

if (WIN32)
  list (APPEND RUNTIME_SRCS_OS_INTERFACE
    os_interface/windows/api.cpp
    os_interface/windows/d3d_sharing_functions.h
    os_interface/windows/d3d9_sharing_functions.cpp
    os_interface/windows/d3d10_11_sharing_functions.cpp
    os_interface/windows/debug_registry_reader.cpp
    os_interface/windows/deferrable_deletion_win.cpp
    os_interface/windows/deferrable_deletion_win.h
    os_interface/windows/device_command_stream.inl
    os_interface/windows/device_factory.cpp
    os_interface/windows/gdi_interface.cpp
    os_interface/windows/gdi_interface.h
    os_interface/windows/options.cpp
    os_interface/windows/os_inc.h
    os_interface/windows/os_interface.cpp
    os_interface/windows/os_interface.h
    os_interface/windows/os_library.cpp
    os_interface/windows/os_library.h
    os_interface/windows/os_time.cpp
    os_interface/windows/os_time.h
    os_interface/windows/registry_reader.h
    os_interface/windows/thk_wrapper.h
    os_interface/windows/wddm.cpp
    os_interface/windows/wddm.h
    os_interface/windows/wddm.inl
    os_interface/windows/wddm_32bit_memory.cpp
    os_interface/windows/wddm_allocation.h
    os_interface/windows/wddm_device_command_stream.inl
    os_interface/windows/wddm_device_command_stream.h
    os_interface/windows/wddm_engine_mapper.h
    os_interface/windows/wddm_engine_mapper.inl
    os_interface/windows/wddm_memory_manager.cpp
    os_interface/windows/wddm_memory_manager.h
    os_interface/windows/windows_defs.h
    os_interface/windows/windows_inc.cpp
    os_interface/windows/windows_wrapper.h
    os_interface/windows/performance_counters_win.cpp
    os_interface/windows/performance_counters_win.h
    os_interface/windows/print.cpp
    os_interface/windows/driver_info.h
    os_interface/windows/driver_info.cpp
  )

  if ("${IGDRCL_OPTION__BITS}" STREQUAL "32" )
    set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH:NO")
  endif ("${IGDRCL_OPTION__BITS}" STREQUAL "32")
endif (WIN32)

if (UNIX)
  list (APPEND RUNTIME_SRCS_OS_INTERFACE
    os_interface/linux/api.cpp
    os_interface/linux/d3d_sharing_functions.h
    os_interface/linux/debug_env_reader.cpp
    os_interface/linux/device_command_stream.inl
    os_interface/linux/device_factory.cpp
    os_interface/linux/drm_32bit_memory.cpp
    os_interface/linux/drm_allocation.h
    os_interface/linux/drm_buffer_object.cpp
    os_interface/linux/drm_buffer_object.h
    os_interface/linux/drm_command_stream.inl
    os_interface/linux/drm_command_stream.h
    os_interface/linux/drm_engine_mapper.h
    os_interface/linux/drm_engine_mapper.inl
    os_interface/linux/drm_null_device.h
    os_interface/linux/drm_gem_close_worker.cpp
    os_interface/linux/drm_gem_close_worker.h
    os_interface/linux/drm_memory_manager.cpp
    os_interface/linux/drm_memory_manager.h
    os_interface/linux/drm_neo.cpp
    os_interface/linux/drm_neo.h
    os_interface/linux/drm_neo_create.cpp
    os_interface/linux/hw_info_config.cpp
    os_interface/linux/hw_info_config.h
    os_interface/linux/linux_inc.cpp
    os_interface/linux/options.cpp
    os_interface/linux/os_inc.h
    os_interface/linux/os_interface.cpp
    os_interface/linux/os_interface.h
    os_interface/linux/os_library.cpp
    os_interface/linux/os_library.h
    os_interface/linux/os_time.cpp
    os_interface/linux/os_time.h
    os_interface/linux/performance_counters_linux.cpp
    os_interface/linux/performance_counters_linux.h
    os_interface/linux/print.cpp
    os_interface/linux/driver_info.cpp
  )
endif (UNIX)

include(enable_gens.cmake)

set (RUNTIME_SRCS
  ${RUNTIME_SRCS_API}
  ${RUNTIME_SRCS_ACCELERATORS}
  ${RUNTIME_SRCS_AUB_MEM_DUMP}
  ${RUNTIME_SRCS_BUILT_INS}
  ${RUNTIME_SRCS_BUILT_IN_KERNELS}
  ${RUNTIME_SRCS_COMMANDS}
  ${RUNTIME_SRCS_COMMAND_QUEUE}
  ${RUNTIME_SRCS_COMMAND_STREAM}
  ${RUNTIME_SRCS_COMPILER_INTERFACE}
  ${RUNTIME_SRCS_CONTEXT}
  ${RUNTIME_SRCS_DEVICE}
  ${RUNTIME_SRCS_DEVICE_QUEUE}
  ${RUNTIME_SRCS_EVENT}
  ${RUNTIME_SRCS_EXECUTION_MODEL}
  ${RUNTIME_SRCS_GEN_COMMON}
  ${RUNTIME_SRCS_GENX}
  ${RUNTIME_SRCS_GTPIN}
  ${RUNTIME_SRCS_HELPERS}
  ${RUNTIME_SRCS_INDIRECT_HEAP}
  ${RUNTIME_SRCS_INSTRUMENTATION}
  ${RUNTIME_SRCS_KERNEL}
  ${RUNTIME_SRCS_MEMORY_MANAGER}
  ${RUNTIME_SRCS_GMM_HELPER}
  ${RUNTIME_SRCS_MEM_OBJ}
  ${RUNTIME_SRCS_OS_INTERFACE}
  ${RUNTIME_SRCS_PLATFORM}
  ${RUNTIME_SRCS_PROGRAM}
  ${RUNTIME_SRCS_SAMPLER}
  ${RUNTIME_SRCS_SCHEDULER}
  ${RUNTIME_SRCS_SKU_INFO}
  ${RUNTIME_SRCS_SHARINGS}
  ${RUNTIME_SRCS_TBX}
  ${RUNTIME_SRCS_UTILITIES}
  CMakeLists.txt
)

# Enable SSE4/AVX2 options for files that need them
if(MSVC)
	set_source_files_properties(command_queue/local_id_gen_avx2.cpp PROPERTIES COMPILE_FLAGS /arch:AVX2)
else()
	set_source_files_properties(command_queue/local_id_gen_avx2.cpp PROPERTIES COMPILE_FLAGS -mavx2)
	set_source_files_properties(command_queue/local_id_gen_sse4.cpp PROPERTIES COMPILE_FLAGS -msse4.2)
endif (MSVC)

# Put Driver version into define
if(NEO_DRIVER_VERSION)
	set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/device/device_caps.cpp PROPERTIES COMPILE_DEFINITIONS NEO_DRIVER_VERSION="${NEO_DRIVER_VERSION}")
endif(NEO_DRIVER_VERSION)

add_library(${NEO_STATIC_LIB_NAME} STATIC $<TARGET_OBJECTS:${BIKSIM_LIB_NAME}>
  ${RUNTIME_SRCS}
)

if (WIN32)
  add_subdirectory(os_interface/windows)
endif(WIN32)

target_link_libraries(${NEO_STATIC_LIB_NAME} elflib)

target_include_directories(${NEO_STATIC_LIB_NAME} PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
  ${GMM_INCLUDE_PATHS}
  ${UMKM_SHAREDDATA_INCLUDE_PATHS}
)

target_include_directories(${NEO_STATIC_LIB_NAME} PUBLIC
	${KHRONOS_HEADERS_DIR}
	${IGDRCL__IGC_INCLUDE_DIR}
	${THIRD_PARTY_DIR}
)

if(GTPIN_HEADERS_DIR)
  target_include_directories(${NEO_STATIC_LIB_NAME} PUBLIC
	${GTPIN_HEADERS_DIR}
  )
endif(GTPIN_HEADERS_DIR)

if (WIN32)
  target_include_directories(${NEO_STATIC_LIB_NAME} PUBLIC
    ${WDK_INCLUDE_PATHS}
    os_interface/windows
  )
  target_compile_definitions(${NEO_STATIC_LIB_NAME} PUBLIC OGL=1)
  target_compile_definitions(${NEO_STATIC_LIB_NAME} PUBLIC INSTR_WIN_UMD=1)

endif (WIN32)

if (UNIX)
  target_compile_definitions(${NEO_STATIC_LIB_NAME} PUBLIC OGL_GEM=1)
  target_include_directories(${NEO_STATIC_LIB_NAME} PUBLIC
    os_interface/linux
    "${LIBDRM_DIR}/include"
  )
endif (UNIX)

target_compile_definitions(${NEO_STATIC_LIB_NAME} PUBLIC DEFAULT_PLATFORM=${DEFAULT_SUPPORTED_PLATFORM})

link_directories(${GMM_LIB_PATHS})

if(NOT GMMUMD_LIB_NAME)
  set(GMMUMD_LIB_NAME "gmm_umd" CACHE STRING "name of gmm static library")
endif()
target_link_libraries(${NEO_STATIC_LIB_NAME} ${GMMUMD_LIB_NAME})

if(WIN32)
  if(GTPIN_HEADERS_DIR)
    set( DEF_FILE "${CMAKE_CURRENT_SOURCE_DIR}/dll/windows/GTPinExports${IGDRCL_OPTION__BITS}.def" )
  else(GTPIN_HEADERS_DIR)
    set( DEF_FILE "${CMAKE_CURRENT_SOURCE_DIR}/dll/windows/OpenCLExports${IGDRCL_OPTION__BITS}.def" )
  endif(GTPIN_HEADERS_DIR)
endif(WIN32)

list(APPEND LIB_FLAGS_DEFINITIONS -DCIF_HEADERS_ONLY_BUILD ${SUPPORTED_GEN_FLAGS_DEFINITONS})

target_compile_definitions(${NEO_STATIC_LIB_NAME} PUBLIC ${LIB_FLAGS_DEFINITIONS})
if(IGC_OCL_ADAPTOR_DIR) # IGC/AdaptorOCL
  target_include_directories("${NEO_STATIC_LIB_NAME}" PUBLIC "${IGC_OCL_ADAPTOR_DIR}")
endif(IGC_OCL_ADAPTOR_DIR)

if(CIF_BASE_DIR)
  target_include_directories("${NEO_STATIC_LIB_NAME}" PUBLIC "${CIF_BASE_DIR}")
endif(CIF_BASE_DIR)

set(IGDRCL_LIB_FLAGS_DEFINITIONS ${LIB_FLAGS_DEFINITIONS} PARENT_SCOPE)
set_target_properties(${NEO_STATIC_LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

set_property(TARGET ${NEO_STATIC_LIB_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS} ${TSAN_FLAGS})

set_vs_props(${NEO_STATIC_LIB_NAME})
set_target_properties(${NEO_STATIC_LIB_NAME} PROPERTIES FOLDER "opencl runtime")

target_include_directories(${NEO_STATIC_LIB_NAME} BEFORE PRIVATE ${HW_SRC_INCLUDE_PATH})

if(${GENERATE_EXECUTABLE})
	set (RUNTIME_SRCS_DLL
	  dll/options.cpp
	  dll/create_command_stream.cpp
	  dll/create_deferred_deleter.cpp
	  dll/create_tbx_sockets.cpp
	  helpers/abort.cpp
	  helpers/debug_helpers.cpp
	  gmm_helper/resource_info.cpp
	  program/evaluate_unhandled_token.cpp
	  "${DEF_FILE}"
	)

	list (APPEND RUNTIME_SRCS_DLL ${HW_SRC_LINK})

	if (UNIX)
		list (APPEND RUNTIME_SRCS_DLL dll/linux/drm_neo_create.cpp)
	endif (UNIX)

	if (WIN32)
	  list (APPEND RUNTIME_SRCS_DLL os_interface/windows/wddm_create.cpp)
	  list (APPEND RUNTIME_SRCS_DLL gmm_helper/page_table_mngr.cpp)
	  list (APPEND RUNTIME_SRCS_DLL gmm_helper/gmm_memory.cpp)
	endif (WIN32)

	list (APPEND RUNTIME_SRCS_DLL api/api.cpp)

	if(GTPIN_HEADERS_DIR)
		list (APPEND RUNTIME_SRCS_DLL gtpin/gtpin_init.cpp)
		foreach(GEN_NUM RANGE ${MAX_GEN} 0 -1)
		  GEN_CONTAINS_PLATFORMS("SUPPORTED" ${GEN_NUM} GENX_HAS_PLATFORMS)
		  if(${GENX_HAS_PLATFORMS})
			list(APPEND RUNTIME_SRCS_GTPIN gen${GEN_NUM}/gtpin_setup_gen${GEN_NUM}.cpp)
		  endif(${GENX_HAS_PLATFORMS})
		endforeach(GEN_NUM)
	endif(GTPIN_HEADERS_DIR)

	add_library(${NEO_DYNAMIC_LIB_NAME} SHARED
	  ${RUNTIME_SRCS_DLL}
	  $<TARGET_OBJECTS:${SHARINGS_ENABLE_LIB_NAME}>
	  $<TARGET_OBJECTS:${BUILTINS_SOURCES_LIB_NAME}>
	  $<TARGET_OBJECTS:${BUILTINS_BINARIES_LIB_NAME}>
	  $<TARGET_OBJECTS:${SCHEDULER_BINARY_LIB_NAME}>
	)

	target_include_directories(${NEO_DYNAMIC_LIB_NAME} BEFORE PRIVATE
	  ${CMAKE_CURRENT_BINARY_DIR}
	  ${HW_SRC_INCLUDE_PATH}
	)

	set_vs_props(${NEO_DYNAMIC_LIB_NAME})

	target_link_libraries(${NEO_DYNAMIC_LIB_NAME} ${NEO_STATIC_LIB_NAME})

	if(HAVE_INSTRUMENTATION)
		target_link_libraries(${NEO_DYNAMIC_LIB_NAME} instrumentation_umd)
	endif()

	if (WIN32)
	  target_include_directories(${NEO_DYNAMIC_LIB_NAME} PUBLIC
		${WDK_INCLUDE_PATHS}
		${GMM_INCLUDE_PATHS}
		${UMKM_SHAREDDATA_INCLUDE_PATHS}
		${INSTRUMENTATION_INCLUDE_PATH}
	  )
	  target_link_libraries(${NEO_DYNAMIC_LIB_NAME} ${NEO_STATIC_LIB_NAME} dxgi Ws2_32.lib)
	else(WIN32)
	  target_include_directories(${NEO_DYNAMIC_LIB_NAME} PUBLIC
		${GMM_INCLUDE_PATHS}
		${UMKM_SHAREDDATA_INCLUDE_PATHS}
		${INSTRUMENTATION_INCLUDE_PATH}
		${CMAKE_CURRENT_SOURCE_DIR}/dll/linux/devices${BRANCH_DIR_SUFFIX}
	  )
	endif (WIN32)

	if (UNIX)
	  target_link_libraries(${NEO_DYNAMIC_LIB_NAME} dl pthread)
	  set_property(TARGET ${NEO_DYNAMIC_LIB_NAME}
		APPEND_STRING PROPERTY LINK_FLAGS " -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/dll/linux/ocl.exports"
	  )
	endif (UNIX)

	set_target_properties(${NEO_DYNAMIC_LIB_NAME} PROPERTIES
	  DEBUG_OUTPUT_NAME "${NEO_DLL_NAME_BASE}${IGDRCL_NAME_POSTFIX}${IGDRCL_OPTION__BITS}"
	  RELEASE_OUTPUT_NAME "${NEO_DLL_NAME_BASE}${IGDRCL_NAME_POSTFIX}${IGDRCL_OPTION__BITS}"
	  RELEASEINTERNAL_OUTPUT_NAME "${NEO_DLL_NAME_BASE}${IGDRCL_NAME_POSTFIX}${IGDRCL_OPTION__BITS}"
	  OUTPUT_NAME "${NEO_DLL_NAME_BASE}${IGDRCL_NAME_POSTFIX}${IGDRCL_OPTION__BITS}"
	)

	set_property(TARGET ${NEO_DYNAMIC_LIB_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS})
	set_target_properties(${NEO_DYNAMIC_LIB_NAME} PROPERTIES FOLDER "opencl runtime")
endif(${GENERATE_EXECUTABLE})

if (WIN32)
  source_group("source files" FILES ${IGDRCL_SRCS_DLL})
  source_group("source files\\api" FILES ${RUNTIME_SRCS_API})
  source_group("source files\\accelerators" FILES ${RUNTIME_SRCS_ACCELERATORS})
  source_group("source files\\aub_mem_dump" FILES ${RUNTIME_SRCS_AUB_MEM_DUMP})
  source_group("source files\\built_ins" FILES ${RUNTIME_SRCS_BUILT_INS})
  source_group("source files\\built_ins\\kernels" FILES ${RUNTIME_SRCS_BUILT_IN_KERNELS})
  source_group("source files\\commands" FILES ${RUNTIME_SRCS_COMMANDS})
  source_group("source files\\command_queue" FILES ${RUNTIME_SRCS_COMMAND_QUEUE})
  source_group("source files\\command_stream" FILES ${RUNTIME_SRCS_COMMAND_STREAM})
  source_group("source files\\compiler_interface" FILES ${RUNTIME_SRCS_COMPILER_INTERFACE})
  source_group("source files\\context" FILES ${RUNTIME_SRCS_CONTEXT})
  source_group("source files\\device" FILES ${RUNTIME_SRCS_DEVICE})
  source_group("source files\\device_queue" FILES ${RUNTIME_SRCS_DEVICE_QUEUE})
  source_group("source files\\event" FILES ${RUNTIME_SRCS_EVENT})
  source_group("source files\\execution_model" FILES ${RUNTIME_SRCS_EXECUTION_MODEL})
  source_group("source files\\gen_common" FILES ${RUNTIME_SRCS_GEN_COMMON})
  source_group("source files\\helpers" FILES ${RUNTIME_SRCS_HELPERS})
  source_group("source files\\indirect_heap" FILES ${RUNTIME_SRCS_INDIRECT_HEAP})
  source_group("source files\\instrumentation" FILES ${RUNTIME_SRCS_INSTRUMENTATION})
  source_group("source files\\kernel" FILES ${RUNTIME_SRCS_KERNEL})
  source_group("source files\\memory_manager" FILES ${RUNTIME_SRCS_MEMORY_MANAGER})
  source_group("source files\\gmm_helper" FILES ${RUNTIME_SRCS_GMM_HELPER})
  source_group("source files\\gtpin" FILES ${RUNTIME_SRCS_GTPIN})
  source_group("source files\\mem_obj" FILES ${RUNTIME_SRCS_MEM_OBJ})
  source_group("source files\\os_interface" FILES ${RUNTIME_SRCS_OS_INTERFACE})
  source_group("source files\\platform" FILES ${RUNTIME_SRCS_PLATFORM})
  source_group("source files\\program" FILES ${RUNTIME_SRCS_PROGRAM})
  source_group("source files\\sampler" FILES ${RUNTIME_SRCS_SAMPLER})
  source_group("source files\\scheduler" FILES ${RUNTIME_SRCS_SCHEDULER})
  source_group("source files\\sku_info" FILES ${RUNTIME_SRCS_SKU_INFO})
  source_group("source files\\sharings" FILES ${RUNTIME_SRCS_SHARINGS})
  source_group("source files\\tbx" FILES ${RUNTIME_SRCS_TBX})
  source_group("source files\\utilities" FILES ${RUNTIME_SRCS_UTILITIES})
endif (WIN32)

if (UNIX)
  if(NOT (TARGET clang-tidy))
    add_custom_target(clang-tidy
      DEPENDS scheduler
    )

    add_custom_command(
      TARGET clang-tidy
      POST_BUILD
      COMMAND echo clang-tidy...
      COMMAND find ${CMAKE_CURRENT_SOURCE_DIR} -name *.cpp -print0 | xargs -0 -I{} -P`nproc` clang-tidy -p ${IGDRCL_BINARY_DIR} {} | tee ${IGDRCL_BINARY_DIR}/clang-tidy.log
      WORKING_DIRECTORY ${IGDRCL_SOURCE_DIR}
    )
  endif(NOT (TARGET clang-tidy))
endif(UNIX)
