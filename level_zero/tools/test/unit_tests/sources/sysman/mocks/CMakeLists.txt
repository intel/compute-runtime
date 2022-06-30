#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysman_env_vars.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysman_device_info.h
               ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman_device_info.cpp
)
