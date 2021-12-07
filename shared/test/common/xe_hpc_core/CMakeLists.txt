#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE_HPC_CORE)

  set(NEO_CORE_TESTS_XE_HPC_CORE
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/simd_helper_tests_xe_hpc_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_traits_xe_hpc_core.h
  )
  set_property(GLOBAL PROPERTY NEO_CORE_TESTS_XE_HPC_CORE ${NEO_CORE_TESTS_XE_HPC_CORE})

  add_subdirectories()

  set(IGDRCL_SRCS_tests_xe_hpc_core_excludes
      ${CMAKE_CURRENT_SOURCE_DIR}/excludes_xe_hpc_core.cpp
  )
  set_property(GLOBAL APPEND PROPERTY IGDRCL_SRCS_tests_excludes ${IGDRCL_SRCS_tests_xe_hpc_core_excludes})

  target_sources(${TARGET_NAME} PRIVATE
                 ${IGDRCL_SRCS_tests_xe_hpc_core_excludes}
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_surface_state_tests_xe_hpc_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_xe_hpc_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_preemption_xe_hpc_core.cpp
                 ${COMPUTE_RUNTIME_ULT_XE_HPC_CORE}
  )

endif()
