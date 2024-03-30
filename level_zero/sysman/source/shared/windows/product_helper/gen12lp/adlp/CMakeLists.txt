#
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_ADLP)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/enable_sysman_product_helper_adlp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_product_helper_adlp.cpp
  )
endif()

