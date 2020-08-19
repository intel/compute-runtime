#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN8)
  target_sources(${TARGET_NAME} PRIVATE
                 ${COMPUTE_RUNTIME_ULT_GEN8}
  )
endif()
if(TESTS_GEN9)
  target_sources(${TARGET_NAME} PRIVATE
                 ${COMPUTE_RUNTIME_ULT_GEN9}
  )
endif()
if(TESTS_GEN11)
  target_sources(${TARGET_NAME} PRIVATE
                 ${COMPUTE_RUNTIME_ULT_GEN11}
  )
endif()
if(TESTS_GEN12LP)
  target_sources(${TARGET_NAME} PRIVATE
                 ${COMPUTE_RUNTIME_ULT_GEN12LP}
  )
  target_include_directories(${TARGET_NAME} PRIVATE ${COMPUTE_RUNTIME_DIR}/level_zero/core/source/gen12lp/definitions${BRANCH_DIR_SUFFIX}/)
endif()

add_subdirectories()
