#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_XE_HPG_CORE)
  add_subdirectories()

  set_property(GLOBAL APPEND PROPERTY NEO_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}")
endif()
