#
# Copyright (C) 2021-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_ADLS)
  set(NEO_CORE_tests_gen12lp_adls_excludes ${CMAKE_CURRENT_SOURCE_DIR}/excludes_adls.cpp)
  set_property(GLOBAL APPEND PROPERTY NEO_CORE_tests_excludes ${NEO_CORE_tests_gen12lp_adls_excludes})

  target_sources(neo_shared_tests PRIVATE
                 ${NEO_CORE_tests_gen12lp_adls_excludes}
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_adls.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_product_helper_adls.cpp
  )

  add_subdirectories()
endif()
