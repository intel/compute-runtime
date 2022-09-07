#
# Copyright (C) 2020-2022 Intel Corporation
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

if(NOT DEFINED L0_BLACK_BOX_OCLOC_HEADER_DIR)
  set(L0_BLACK_BOX_OCLOC_HEADER_DIR "${NEO_SOURCE_DIR}/shared/offline_compiler/source")
endif()

target_link_libraries(${L0_BLACK_BOX_TEST_SHARED_LIB} PUBLIC ocloc_lib)
target_include_directories(${L0_BLACK_BOX_TEST_SHARED_LIB} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/common ${L0_BLACK_BOX_OCLOC_HEADER_DIR})

set_target_properties(${L0_BLACK_BOX_TEST_SHARED_LIB} PROPERTIES FOLDER ${L0_BLACK_BOX_TEST_PROJECT_FOLDER})

set(TEST_TARGETS
    zello_commandlist_immediate
    zello_copy
    zello_copy_fence
    zello_copy_image
    zello_copy_kernel_printf
    zello_copy_only
    zello_copy_tracing
    zello_debug_info
    zello_dynamic_link
    zello_dyn_local_arg
    zello_events
    zello_fence
    zello_function_pointers_cl
    zello_host_pointer
    zello_image
    zello_image_view
    zello_immediate
    zello_ipc_copy_dma_buf
    zello_ipc_copy_dma_buf_p2p
    zello_multidev
    zello_printf
    zello_p2p_copy
    zello_scratch
    zello_timestamp
    zello_world_global_work_offset
    zello_world_gpu
    zello_world_jitc_ocloc
    zello_world_usm
)

include_directories(common)

foreach(TEST_NAME ${TEST_TARGETS})
  if(MSVC)
    if(${TEST_NAME} STREQUAL "zello_ipc_copy_dma_buf")
      continue()
    endif()
    if(${TEST_NAME} STREQUAL "zello_ipc_copy_dma_buf_p2p")
      continue()
    endif()
  endif()

  add_executable(${TEST_NAME} ${TEST_NAME}.cpp)

  set_target_properties(${TEST_NAME}
                        PROPERTIES
                        VS_DEBUGGER_COMMAND "$(TargetPath)"
                        VS_DEBUGGER_COMMAND_ARGUMENTS ""
                        VS_DEBUGGER_WORKING_DIRECTORY "${TargetDir}"
  )

  add_dependencies(${TEST_NAME} ${TARGET_NAME_L0})
  target_link_libraries(${TEST_NAME} PUBLIC ${TARGET_NAME_L0})

  set_target_properties(${TEST_NAME} PROPERTIES FOLDER ${L0_BLACK_BOX_TEST_PROJECT_FOLDER})

  add_dependencies(${TEST_NAME} ${L0_BLACK_BOX_TEST_SHARED_LIB})
  target_link_libraries(${TEST_NAME} PUBLIC ${L0_BLACK_BOX_TEST_SHARED_LIB})
endforeach()

add_subdirectories()

create_project_source_tree(${L0_BLACK_BOX_TEST_SHARED_LIB})
