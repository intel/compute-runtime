#
# Copyright (C) 2023-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_ARL)
  set(NEO_CORE_tests_xe_hpg_core_arl_excludes ${CMAKE_CURRENT_SOURCE_DIR}/excludes_xe_hpg_core_arl.cpp)
  set_property(GLOBAL APPEND PROPERTY NEO_CORE_tests_excludes ${NEO_CORE_tests_xe_hpg_core_arl_excludes})

  target_sources(neo_shared_tests PRIVATE
                 ${NEO_CORE_tests_xe_hpg_core_arl_excludes}
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/ail_tests_arl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_aot_config_tests_arl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/product_helper_tests_arl.cpp
  )

  add_subdirectories()
endif()
