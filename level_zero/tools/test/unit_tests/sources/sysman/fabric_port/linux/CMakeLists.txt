#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
    target_sources(${TARGET_NAME}
        PRIVATE
           ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
           ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman_fabric_port.cpp
    )
endif()
