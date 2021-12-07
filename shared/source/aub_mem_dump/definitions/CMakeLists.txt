#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

get_property(NEO_CORE_AUB_MEM_DUMP GLOBAL PROPERTY NEO_CORE_AUB_MEM_DUMP)

list(APPEND NEO_CORE_AUB_MEM_DUMP
     ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
     ${CMAKE_CURRENT_SOURCE_DIR}/aub_services.h
)

set_property(GLOBAL PROPERTY NEO_CORE_AUB_MEM_DUMP ${NEO_CORE_AUB_MEM_DUMP})
add_subdirectories()
