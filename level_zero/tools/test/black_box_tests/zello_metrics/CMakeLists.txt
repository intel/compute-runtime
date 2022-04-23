#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  set(L0_ZELLO_METRICS_PROJECT_FOLDER "ze_intel_gpu/black_box_tests")
  set(TEST_TARGET zello_metrics)

  add_executable(zello_metrics zello_metrics_collector.cpp
                 zello_metrics_execution_context.cpp
                 zello_metrics_workload.cpp
                 zello_metrics.cpp
                 zello_metrics_util.cpp
                 zello_metrics_util.h
                 zello_metrics.h
  )

  if(BUILD_LEVEL_ZERO_LOADER)
    add_dependencies(${TEST_TARGET} ze_loader)
    target_link_libraries(${TEST_TARGET} ${NEO_BINARY_DIR}/lib/libze_loader.so pthread)
  else()
    target_link_libraries(${TEST_TARGET} PUBLIC ${TARGET_NAME_L0} pthread)
  endif()
  set_target_properties(${TEST_TARGET} PROPERTIES FOLDER ${L0_ZELLO_METRICS_PROJECT_FOLDER})
endif()

add_subdirectories()
