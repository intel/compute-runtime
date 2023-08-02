#
# Copyright (C) 2022-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_MTL)
  set(NEO_CORE_tests_xe_hpg_core_mtl_excludes ${CMAKE_CURRENT_SOURCE_DIR}/excludes_xe_hpg_core_mtl.cpp)
  set_property(GLOBAL APPEND PROPERTY NEO_CORE_tests_excludes ${NEO_CORE_tests_xe_hpg_core_mtl_excludes})

  target_sources(neo_shared_tests PRIVATE
                 ${NEO_CORE_tests_xe_hpg_core_mtl_excludes}
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/ail_tests_mtl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/excludes_xe_hpg_core_mtl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_aot_config_tests_mtl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/product_config_helper_tests_mtl.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/product_helper_tests_mtl.cpp
  )

  add_subdirectories()
endif()
