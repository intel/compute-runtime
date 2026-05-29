#
# Copyright (C) 2023-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_shared_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_tests_base.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_tests_base.h

               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_gen12lp_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_dg2_g10_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_dg2_g11_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_dg2_g12_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_pvc_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_pvc_vg_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_mtl_u_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_mtl_h_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_arl_h_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_bmg_g21_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_bmg_g31_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_lnl_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_ptl_h_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_ptl_u_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_wcl_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_nvl_s_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_nvl_u_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_nvl_p_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_cri_tests.cpp
)

add_subdirectories()
