#
# Copyright (C) 2019-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_ADDITIONAL_FILES_GEN12LP
    ${CMAKE_CURRENT_SOURCE_DIR}/gen12lp/definitions${BRANCH_DIR_SUFFIX}command_queue_helpers_gen12lp.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/gen12lp/definitions${BRANCH_DIR_SUFFIX}hardware_commands_helper_gen12lp.inl
)
include_directories(${NEO_SOURCE_DIR}/opencl/source/gen12lp/definitions${BRANCH_DIR_SUFFIX})

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_ADDITIONAL_FILES_GEN12LP})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_ADDITIONAL_FILES_GEN12LP ${RUNTIME_SRCS_ADDITIONAL_FILES_GEN12LP})
