#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_tests_mocks
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_command_stream_receiver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_command_stream_receiver.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_direct_submission_hw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_direct_submission_diagnostic_collector.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_dispatch_kernel_encoder_interface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_dispatch_kernel_encoder_interface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_device.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_device.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_os_library.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ult_device_factory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ult_device_factory.h
)

if(WIN32)
  list(APPEND NEO_CORE_tests_mocks
       ${CMAKE_CURRENT_SOURCE_DIR}/windows/mock_wddm_direct_submission.h
  )
else()
  list(APPEND NEO_CORE_tests_mocks
       ${CMAKE_CURRENT_SOURCE_DIR}/linux/mock_drm_memory_manager.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/linux/mock_drm_memory_manager.h
  )
endif()

set_property(GLOBAL PROPERTY NEO_CORE_tests_mocks ${NEO_CORE_tests_mocks})
