#
# Copyright (C) 2023-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_RELEASE_HELPER
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}release_definitions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_common_xe_lpg.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_common_xe_hpg.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_common_gen12lp.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_tgl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_rkl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_adl_s.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_adl_p.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_adl_n.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_dg1.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_dg2_g10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_dg2_g11.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_dg2_g12.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_pvc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_pvc_vg.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_mtl_u.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_mtl_h.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_arl_h.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_bmg_g21.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_bmg_g31.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_lnl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_ptl_h.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_ptl_u.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_wcl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_nvl_s.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_nvl_u.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_nvl_p.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_cri.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_common_xe2.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_common_xe3_and_later.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_common_xe3_lpg.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_common_xe3p.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_common_xe3p_lpg.inl
)

set_property(GLOBAL APPEND PROPERTY NEO_CORE_ENABLE_RELEASE_HELPER
             ${CMAKE_CURRENT_SOURCE_DIR}/enable_release_helper_xe.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/enable_release_helper_xe2.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/enable_release_helper_xe3.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}enable_release_helper_xe3p.cpp
)
set_property(GLOBAL APPEND PROPERTY NEO_CORE_RELEASE_HELPER ${NEO_CORE_RELEASE_HELPER})

add_subdirectories()
get_property(NEO_CORE_ENABLE_RELEASE_HELPER GLOBAL PROPERTY NEO_CORE_ENABLE_RELEASE_HELPER)
set_property(GLOBAL APPEND PROPERTY NEO_CORE_SRCS_LINK ${NEO_CORE_ENABLE_RELEASE_HELPER})
