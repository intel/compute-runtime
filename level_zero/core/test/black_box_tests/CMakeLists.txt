#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_BLACK_BOX_TEST_PROJECT_FOLDER "ze_intel_gpu/black_box_tests")
set(TEST_TARGETS
    zello_timestamp
    zello_world_gpu
    zello_world_jitc_ocloc
    zello_ipc_copy_dma_buf
    zello_ipc_copy_dma_buf_p2p
    zello_copy
    zello_copy_only
    zello_copy_fence
    zello_copy_image
    zello_world_usm
    zello_world_global_work_offset
    zello_scratch
    zello_fence
    zello_printf
    zello_image
    zello_image_view
    zello_dynamic_link
    zello_events
    zello_immediate
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
    if(${TEST_NAME} STREQUAL "zello_world_global_work_offset")
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
endforeach()

target_link_libraries(zello_world_jitc_ocloc PUBLIC ocloc_lib)
target_link_libraries(zello_scratch PUBLIC ocloc_lib)
target_link_libraries(zello_fence PUBLIC ocloc_lib)
target_link_libraries(zello_printf PUBLIC ocloc_lib)
target_link_libraries(zello_image_view PUBLIC ocloc_lib)
target_link_libraries(zello_dynamic_link PUBLIC ocloc_lib)
if(UNIX)
  target_link_libraries(zello_world_global_work_offset PUBLIC ocloc_lib)
endif()

add_subdirectories()
