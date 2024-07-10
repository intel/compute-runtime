#
# Copyright (C) 2023-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(ocloc_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_argument_helper.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_multi_command.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_ocloc_concat.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_ocloc_fcl_facade.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_ocloc_fcl_facade.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_ocloc_igc_facade.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_ocloc_igc_facade.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_ocloc_supported_devices_helper.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_offline_compiler.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_offline_compiler.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_offline_linker.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_sip_ocloc_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_iga_dll.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_iga_dll.h
)

