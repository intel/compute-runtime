#
# Copyright (C) 2019-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_COMMAND_STREAM
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_receiver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_receiver.h
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_subcapture_status.h
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver.h
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_bdw_plus.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_tgllp_plus.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/csr_definitions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/csr_deps.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/csr_deps.h
    ${CMAKE_CURRENT_SOURCE_DIR}/csr_properties_flags.h
    ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}/command_stream_receiver_hw_ext.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}/stream_properties.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/device_command_stream.h
    ${CMAKE_CURRENT_SOURCE_DIR}/experimental_command_buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/experimental_command_buffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/experimental_command_buffer.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/linear_stream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/linear_stream.h
    ${CMAKE_CURRENT_SOURCE_DIR}/preemption.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/preemption.h
    ${CMAKE_CURRENT_SOURCE_DIR}/preemption.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/preemption_mode.h
    ${CMAKE_CURRENT_SOURCE_DIR}/scratch_space_controller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scratch_space_controller.h
    ${CMAKE_CURRENT_SOURCE_DIR}/scratch_space_controller_base.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scratch_space_controller_base.h
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/stream_properties.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/stream_properties.h
    ${CMAKE_CURRENT_SOURCE_DIR}/stream_property.h
    ${CMAKE_CURRENT_SOURCE_DIR}/submissions_aggregator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/submissions_aggregator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tbx_command_stream_receiver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tbx_command_stream_receiver.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tbx_command_stream_receiver_hw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tbx_command_stream_receiver_hw.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/tbx_stream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thread_arbitration_policy.h
)

if(SUPPORT_XEHP_PLUS)
  list(APPEND NEO_CORE_COMMAND_STREAM
       ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_xehp_plus.inl
       ${CMAKE_CURRENT_SOURCE_DIR}/preemption_xehp_plus.inl
       ${CMAKE_CURRENT_SOURCE_DIR}/scratch_space_controller_xehp_plus.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/scratch_space_controller_xehp_plus.h
  )
endif()

set_property(GLOBAL PROPERTY NEO_CORE_COMMAND_STREAM ${NEO_CORE_COMMAND_STREAM})
add_subdirectories()
