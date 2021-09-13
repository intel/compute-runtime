#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_GEN_COMMON
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}aub_mapper.h
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}reg_configs_common.h
)

set_property(GLOBAL PROPERTY NEO_CORE_GEN_COMMON ${NEO_CORE_GEN_COMMON})
