#
# Copyright (C) 2022-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_KBL)
  set(NEO_CORE_tests_gen9_kbl_excludes ${CMAKE_CURRENT_SOURCE_DIR}/excludes_kbl.cpp)
  set_property(GLOBAL APPEND PROPERTY NEO_CORE_tests_excludes ${NEO_CORE_tests_gen9_kbl_excludes})
  target_sources(neo_shared_tests PRIVATE
                 ${NEO_CORE_tests_gen9_kbl_excludes}
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_product_helper_kbl.cpp
  )

  add_subdirectories()
endif()
