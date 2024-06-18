#
# Copyright (C) 2020-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  if(LIBGENL_FOUND)

    set(L0_TESTS_SYSMAN_IAF_NLAPI_LINUX
        ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman_nl_api.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mock_nl_dll.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mock_nl_dll.h
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman_iaf_nl_api.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mock_nl_api.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mock_iaf_nl_api.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mock_nl_api.h
        ${CMAKE_CURRENT_SOURCE_DIR}/mock_iaf_nl_api.h
    )

    target_sources(${TARGET_NAME}
                   PRIVATE
                   ${L0_TESTS_SYSMAN_IAF_NLAPI_LINUX}
    )
  endif()

endif()
