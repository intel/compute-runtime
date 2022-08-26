#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  if(LIBGENL_FOUND)

    set(L0_TESTS_TOOLS_SYSMAN_IAF_NLAPI_LINUX
        ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman_nl_api.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mock_nl_dll.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mock_nl_dll.h
    )

    add_subdirectories()

    target_sources(${TARGET_NAME}
                   PRIVATE
                   ${L0_TESTS_TOOLS_SYSMAN_IAF_NLAPI_LINUX}
    )
  endif()

endif()
