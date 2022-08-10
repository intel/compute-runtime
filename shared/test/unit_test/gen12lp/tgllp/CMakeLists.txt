#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_TGLLP)
  set(NEO_SHARED_tests_genlp12_tgllp ${CMAKE_CURRENT_SOURCE_DIR}/excludes_gen12lp_tgllp.cpp)

  target_sources(neo_shared_tests PRIVATE
                 ${NEO_SHARED_tests_genlp12_tgllp}
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_tests_tgllp.cpp
  )

  add_subdirectories()
endif()
