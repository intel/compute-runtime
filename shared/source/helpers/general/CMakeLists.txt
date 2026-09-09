#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_HELPERS
        ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/local_id_gen.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/stream_copy.cpp
)

set_property(GLOBAL APPEND PROPERTY NEO_CORE_HELPERS ${NEO_CORE_HELPERS})
