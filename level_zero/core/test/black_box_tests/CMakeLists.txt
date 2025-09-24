#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_BLACK_BOX_TEST_PROJECT_FOLDER "ze_intel_gpu/black_box_tests")

set(L0_BLACK_BOX_TEST_SHARED_LIB "zello_shared_lib")

add_library(${L0_BLACK_BOX_TEST_SHARED_LIB} STATIC
            ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
            ${CMAKE_CURRENT_SOURCE_DIR}/common/zello_common.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/common/zello_common.h
            ${CMAKE_CURRENT_SOURCE_DIR}/common/zello_compile.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/common/zello_compile.h
)

if(UNIX)
  target_sources(${L0_BLACK_BOX_TEST_SHARED_LIB}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/common/zello_ipc_common.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/common/zello_ipc_common.cpp
  )
endif()

if(NOT DEFINED L0_BLACK_BOX_OCLOC_HEADER_DIR)
  set(L0_BLACK_BOX_OCLOC_HEADER_DIR "${NEO_SOURCE_DIR}/shared/offline_compiler/source")
endif()

target_link_libraries(${L0_BLACK_BOX_TEST_SHARED_LIB} PUBLIC ocloc_lib)
target_include_directories(${L0_BLACK_BOX_TEST_SHARED_LIB} PUBLIC
                           ${CMAKE_CURRENT_SOURCE_DIR}/common
                           ${L0_BLACK_BOX_OCLOC_HEADER_DIR}
                           ${NEO_SOURCE_DIR}/level_zero/include
)

set_target_properties(${L0_BLACK_BOX_TEST_SHARED_LIB} PROPERTIES FOLDER ${L0_BLACK_BOX_TEST_PROJECT_FOLDER})

if("${EXTERNAL_L0_LOADER_PATH}" STREQUAL "")
  set(L0_BB_LINK_TARGET ${TARGET_NAME_L0})
  message(STATUS "Linking black box tests against driver library")
else()
  set(L0_BB_LINK_TARGET ${EXTERNAL_L0_LOADER_PATH})
  message(STATUS "Linking black box tests against external ${EXTERNAL_L0_LOADER_PATH}")
endif()

set(TEST_TARGETS
    zello_arg_slm
    zello_atomic_inc
    zello_bindless_kernel
    zello_commandlist_immediate
    zello_copy
    zello_dp_support
    zello_copy_fence
    zello_copy_image
    zello_copy_kernel_printf
    zello_copy_only
    zello_debug_info
    zello_dynamic_link
    zello_dyn_local_arg
    zello_events
    zello_fence
    zello_fill
    zello_function_pointers_cl
    zello_global_bindless_kernel
    zello_graph
    zello_host_function
    zello_host_pointer
    zello_image
    zello_image_view
    zello_immediate
    zello_ipc_copy_dma_buf
    zello_ipc_copy_dma_buf_p2p
    zello_memFree_callback
    zello_multidev
    zello_printf
    zello_p2p_copy
    zello_sandbox
    zello_scratch
    zello_timestamp
    zello_world_global_work_offset
    zello_world_gpu
    zello_world_jitc_ocloc
    zello_world_usm
    zello_fabric
)

include_directories(common)

foreach(TEST_NAME ${TEST_TARGETS})
  if(WIN32)
    if(${TEST_NAME} STREQUAL "zello_host_ipc_copy_dma_buf")
      continue()
    endif()
    if(${TEST_NAME} STREQUAL "zello_ipc_copy_dma_buf")
      continue()
    endif()
    if(${TEST_NAME} STREQUAL "zello_ipc_copy_dma_buf_p2p")
      continue()
    endif()
    if(${TEST_NAME} STREQUAL "zello_export_import_memory")
      continue()
    endif()
    if(${TEST_NAME} STREQUAL "zello_host_export_import_memory")
      continue()
    endif()
    if(${TEST_NAME} STREQUAL "zello_ipc_event")
      continue()
    endif()
  endif()

  set(EXTRA_SOURCES "")
  if(${TEST_NAME} STREQUAL "zello_bindless_kernel")
    set(EXTRA_SOURCES ${NEO_SOURCE_DIR}/level_zero/core/source/image/image_format_desc_helper.cpp)
  endif()

  add_executable(${TEST_NAME} ${TEST_NAME}.cpp
                 ${EXTRA_SOURCES}
  )

  set_target_properties(${TEST_NAME}
                        PROPERTIES
                        VS_DEBUGGER_COMMAND "$(TargetPath)"
                        VS_DEBUGGER_COMMAND_ARGUMENTS ""
                        VS_DEBUGGER_WORKING_DIRECTORY "${TargetDir}"
  )

  target_link_libraries(${TEST_NAME} PUBLIC ${L0_BB_LINK_TARGET})

  set_target_properties(${TEST_NAME} PROPERTIES FOLDER ${L0_BLACK_BOX_TEST_PROJECT_FOLDER})

  add_dependencies(${TEST_NAME} ${L0_BLACK_BOX_TEST_SHARED_LIB})
  target_link_libraries(${TEST_NAME} PUBLIC ${L0_BLACK_BOX_TEST_SHARED_LIB})
endforeach()

add_subdirectories()

create_project_source_tree(${L0_BLACK_BOX_TEST_SHARED_LIB})
