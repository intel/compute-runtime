#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_DEBUGGER
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/debugger.h

)

set_property(GLOBAL PROPERTY NEO_CORE_DEBUGGER ${NEO_CORE_DEBUGGER})
