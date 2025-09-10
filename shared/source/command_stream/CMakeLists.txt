#
# Copyright (C) 2019-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_COMMAND_STREAM
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_receiver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_receiver.h
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_receiver_hw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_receiver_hw_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_subcapture_status.h
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver.h
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_simulated_common_hw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_simulated_common_hw_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_simulated_hw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_with_aub_dump.h
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_with_aub_dump.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/create_command_stream_impl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/create_command_stream_impl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/csr_definitions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/csr_deps.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/csr_deps.h
    ${CMAKE_CURRENT_SOURCE_DIR}/csr_properties_flags.h
    ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}command_stream_receiver_hw_ext.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}stream_properties.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/device_command_stream.h
    ${CMAKE_CURRENT_SOURCE_DIR}/host_function.h
    ${CMAKE_CURRENT_SOURCE_DIR}/host_function.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/host_function_enablers.inl
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
    ${CMAKE_CURRENT_SOURCE_DIR}/stream_properties.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/stream_properties.h
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}stream_properties_extra.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/stream_property.h
    ${CMAKE_CURRENT_SOURCE_DIR}/submission_status.h
    ${CMAKE_CURRENT_SOURCE_DIR}/submissions_aggregator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/submissions_aggregator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tag_allocation_layout.h
    ${CMAKE_CURRENT_SOURCE_DIR}/task_count_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tbx_command_stream_receiver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tbx_command_stream_receiver.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tbx_command_stream_receiver_hw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tbx_command_stream_receiver_hw.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/thread_arbitration_policy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/queue_throttle.h
    ${CMAKE_CURRENT_SOURCE_DIR}/queue_throttle.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transfer_direction.h
    ${CMAKE_CURRENT_SOURCE_DIR}/wait_status.h
)

if(SUPPORT_XEHP_AND_LATER)
  list(APPEND NEO_CORE_COMMAND_STREAM
       ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_receiver_hw_xehp_and_later.inl
       ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_xehp_and_later.inl
       ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_simulated_common_hw_xehp_and_later.inl
       ${CMAKE_CURRENT_SOURCE_DIR}/scratch_space_controller_xehp_and_later.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/scratch_space_controller_xehp_and_later.h
       ${CMAKE_CURRENT_SOURCE_DIR}/tbx_command_stream_receiver_xehp_and_later.inl
  )
endif()

if(SUPPORT_XE_HPG_CORE OR SUPPORT_XE_HPC_CORE)
  list(APPEND NEO_CORE_COMMAND_STREAM
       ${CMAKE_CURRENT_SOURCE_DIR}/preemption_xe.inl
  )
endif()

if(SUPPORT_DG2_AND_LATER)
  list(APPEND NEO_CORE_COMMAND_STREAM
       ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_dg2_and_later.inl
  )
endif()

if(NOT SUPPORTED_HEAPLESS)
  list(APPEND NEO_CORE_COMMAND_STREAM
       ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_heap_addressing.inl
  )
endif()

if(SUPPORT_XE2_AND_LATER)
  list(APPEND NEO_CORE_COMMAND_STREAM
       ${CMAKE_CURRENT_SOURCE_DIR}/preemption_xe2_and_later.inl
  )
endif()

set_property(GLOBAL PROPERTY NEO_CORE_COMMAND_STREAM ${NEO_CORE_COMMAND_STREAM})
add_subdirectories()
