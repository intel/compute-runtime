#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_BUILT_INS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/sip_kernel_type.h
)

set_property(GLOBAL PROPERTY NEO_CORE_BUILT_INS ${NEO_CORE_BUILT_INS})