#
# Copyright (C) 2022-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_DG2)
  set(NEO_CORE_tests_xe_hpg_core_dg2_excludes ${CMAKE_CURRENT_SOURCE_DIR}/excludes_xe_hpg_core_dg2.cpp)
  if(WIN32 OR(UNIX AND NOT DISABLE_WDDM_LINUX))
    list(APPEND NEO_CORE_tests_xe_hpg_core_dg2_excludes ${CMAKE_CURRENT_SOURCE_DIR}/excludes_wddm_xe_hpg_core_dg2.cpp)
  endif()
  set_property(GLOBAL APPEND PROPERTY NEO_CORE_tests_excludes ${NEO_CORE_tests_xe_hpg_core_dg2_excludes})

  target_sources(neo_shared_tests PRIVATE
                 ${NEO_CORE_tests_xe_hpg_core_dg2_excludes}
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/ail_tests_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/cache_policy_tests_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/compute_mode_tests_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/compiler_product_helper_dg2_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/device_binary_format_ar_tests_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/device_factory_tests_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/device_tests_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/gfx_core_helper_tests_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/memory_manager_tests_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/product_config_helper_tests_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sampler_tests_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_dispatch_kernel_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_product_helper_dg2.cpp
  )

  add_subdirectories()
endif()
