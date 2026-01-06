#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/ze_mutable_command_list_exp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_mutable_cmdlist_control_flow_ext.h
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_mutable_cmdlist_dispatch_ext.h
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_mutable_cmdlist_ext.h
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_mutable_cmdlist_serialization_ext.h
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_mutable_cmdlist_temp_mem_ext.h
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_mutable_cmdlist_variable_ext.h
               ${CMAKE_CURRENT_SOURCE_DIR}/zex_mutable_cmdlist_variable_info_ext.h
)
