#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set_property(GLOBAL PROPERTY SHARED_SRCS_ADDITIONAL_FILES_GEN12LP ${SHARED_SRCS_ADDITIONAL_FILES_GEN12LP})

if(SUPPORT_GEN12LP)
  add_subdirectories()
endif()
