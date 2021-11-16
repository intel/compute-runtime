#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_ADDITIONAL_FILES_XE_HPC_CORE
    ${CMAKE_CURRENT_SOURCE_DIR}/xe_hpc_core/definitions${BRANCH_DIR_SUFFIX}gtpin_setup_xe_hpc_core.inl
)
include_directories(${NEO_SOURCE_DIR}/opencl/source/xe_hpc_core/definitions${BRANCH_DIR_SUFFIX})

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_ADDITIONAL_FILES_XE_HPC_CORE})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_ADDITIONAL_FILES_XE_HPC_CORE ${RUNTIME_SRCS_ADDITIONAL_FILES_XE_HPC_CORE})
