#
# Copyright (C) 2017-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_gtpin
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/gtpin_tests.cpp
)
macro(macro_for_each_gen)
  list(APPEND IGDRCL_SRCS_tests_gtpin ${NEO_SOURCE_DIR}/opencl/source/${GEN_TYPE_LOWER}/gtpin_setup_${GEN_TYPE_LOWER}.cpp)
endmacro()
apply_macro_for_each_gen("TESTED")
if(NOT DISABLED_GTPIN_SUPPORT)
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gtpin})
endif()
