#
# Copyright (C) 2018-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_shared_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/sku_info_base_reference.h
               ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}sku_info_transfer_tests.cpp
)

if(WIN32)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}sku_info_receiver_tests.cpp
  )
endif()
