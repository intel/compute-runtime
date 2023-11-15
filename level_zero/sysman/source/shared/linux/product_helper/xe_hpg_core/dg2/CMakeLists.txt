#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_DG2)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_product_helper_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/enable_sysman_product_helper_dg2.cpp
  )
endif()

