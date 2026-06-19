#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  if(LIBTRACEFS_FOUND)

    set(L0_TESTS_SYSMAN_TRACEFS_API_LINUX
        ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman_tracefs_api.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mock_tracefs_os_library.cpp
    )

    target_sources(${TARGET_NAME}
                   PRIVATE
                   ${L0_TESTS_SYSMAN_TRACEFS_API_LINUX}
    )

  endif()

endif()

add_subdirectories()
