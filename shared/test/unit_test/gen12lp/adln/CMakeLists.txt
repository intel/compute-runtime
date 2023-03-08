#
# Copyright (C) 2022-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_ADLN)
  set(NEO_CORE_tests_gen12lp_adln_excludes ${CMAKE_CURRENT_SOURCE_DIR}/excludes_adln.cpp)
  set_property(GLOBAL APPEND PROPERTY NEO_CORE_tests_excludes ${NEO_CORE_tests_gen12lp_adln_excludes})

  target_sources(neo_shared_tests PRIVATE
                 ${NEO_CORE_tests_gen12lp_adln_excludes}
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_adln.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_product_helper_adln.cpp
  )

  add_subdirectories()
endif()