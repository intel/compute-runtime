#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  if(LIBGENL_FOUND)
    target_sources(${TARGET_NAME}
                   PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman_nl_api.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/mock_nl_api.cpp
    )
  endif()
endif()
