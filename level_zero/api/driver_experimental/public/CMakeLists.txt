#
# Copyright (C) 2020-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/ze_bindless_image_exp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_api.h
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_cmdlist.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_cmdlist.h
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_context.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_context.h
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_common.h
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_driver.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_driver.h
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_event.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_event.h
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_memory.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_memory.h
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_module.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_module.h
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_metric.h
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_metric.cpp
)
