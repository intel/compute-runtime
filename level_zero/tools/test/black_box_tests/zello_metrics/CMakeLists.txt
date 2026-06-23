#
# Copyright (C) 2022-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(ZELLO_METRICS_TARGET zello_metrics)
set(L0_ZELLO_METRICS_PROJECT_FOLDER "ze_intel_gpu/black_box_tests")
add_executable(${ZELLO_METRICS_TARGET})
target_sources(${ZELLO_METRICS_TARGET}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/zello_metrics_collector.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/zello_metrics_execution_context.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/zello_metrics_workload.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/zello_metrics.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/zello_metrics_util.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/zello_metrics_export.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/zello_metrics_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/zello_metrics_programmable_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/zello_metrics_util.h
               ${CMAKE_CURRENT_SOURCE_DIR}/zello_metrics.h
)

add_subdirectories()

target_link_libraries(${ZELLO_METRICS_TARGET} PUBLIC ${L0_LOADER_LIBRARY})
if(UNIX)
  target_link_libraries(${ZELLO_METRICS_TARGET} PUBLIC pthread)
endif()
set_target_properties(${ZELLO_METRICS_TARGET} PROPERTIES FOLDER ${L0_ZELLO_METRICS_PROJECT_FOLDER})
target_include_directories(${ZELLO_METRICS_TARGET} PUBLIC ${NEO_SOURCE_DIR}/level_zero/include)
