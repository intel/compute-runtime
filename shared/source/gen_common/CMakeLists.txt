#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_GEN_COMMON
    ${CMAKE_CURRENT_SOURCE_DIR}/reg_configs_common.h
)

set_property(GLOBAL PROPERTY NEO_CORE_GEN_COMMON ${NEO_CORE_GEN_COMMON})
