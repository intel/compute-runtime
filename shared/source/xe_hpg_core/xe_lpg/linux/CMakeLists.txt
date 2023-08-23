#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set_property(GLOBAL APPEND PROPERTY NEO_CORE_HELPERS
             ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
             ${CMAKE_CURRENT_SOURCE_DIR}/product_helper_xe_lpg_linux.inl
)
add_subdirectories()
