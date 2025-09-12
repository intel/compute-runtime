/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <map>
#include <string>
#include <cstdint>
#include <vector>

#pragma once

namespace AOT {

enum PRODUCT_CONFIG : uint32_t {
    UNKNOWN_ISA = 0,
    TGL = 0x03000000,
    RKL = 0x03004000,
    ADL_S = 0x03008000,
    ADL_P = 0x0300c000,
    ADL_N = 0x03010000,
    DG1 = 0x03028000,
    XEHP_SDV = 0x030c8004,
    DG2_G10_A0 = 0x030dc000,
    DG2_G10_A1 = 0x030dc001,
    DG2_G10_B0 = 0x030dc004,
    DG2_G10_C0 = 0x030dc008,
    DG2_G11_A0 = 0x030e0000,
    DG2_G11_B0 = 0x030e0004,
    DG2_G11_B1 = 0x030e0005,
    DG2_G12_A0 = 0x030e4000,
    PVC_XL_A0 = 0x030f0000,
    PVC_XL_A0P = 0x030f0001,
    PVC_XT_A0 = 0x030f0003,
    PVC_XT_B0 = 0x030f0005,
    PVC_XT_B1 = 0x030f0006,
    PVC_XT_C0 = 0x030f0007,
    PVC_XT_C0_VG = 0x030f4007,
    MTL_U_A0 = 0x03118000,
    MTL_U_B0 = 0x03118004,
    MTL_H_A0 = 0x0311c000,
    MTL_H_B0 = 0x0311c004,
    ARL_H_A0 = 0x03128000,
    ARL_H_B0 = 0x03128004,
    BMG_G21_A0 = 0x05004000,
    BMG_G21_A1_RESERVED = 0x05004001,
    BMG_G21_B0_RESERVED = 0x05004004,
    BMG_G31_A0 = 0x05008000,
    LNL_A0 = 0x05010000,
    LNL_A1 = 0x05010001,
    LNL_B0 = 0x05010004,
    PTL_H_A0 = 0x07800000,
    PTL_H_B0 = 0x07800004,
    PTL_U_A0 = 0x07804000,
    PTL_U_A1 = 0x07804001,
    WCL_A0 = 0x0780c000,
    WCL_A1 = 0x0780c001,
    CONFIG_MAX_PLATFORM
};

enum RELEASE : uint32_t {
    UNKNOWN_RELEASE = 0,
    XE_LP_RELEASE,
    XE_HP_RELEASE,
    XE_HPG_RELEASE,
    XE_HPC_RELEASE,
    XE_HPC_VG_RELEASE,
    XE_LPG_RELEASE,
    XE_LPGPLUS_RELEASE,
    XE2_HPG_RELEASE,
    XE2_LPG_RELEASE,
    XE3_LPG_RELEASE,
    RELEASE_MAX
};

enum FAMILY : uint32_t {
    UNKNOWN_FAMILY = 0,
    XE_FAMILY,
    XE2_FAMILY,
    XE3_FAMILY,
    FAMILY_MAX
};

inline const std::map<std::string, FAMILY> familyAcronyms = {
#ifdef SUPPORT_AOT_XE
    {"xe", XE_FAMILY},
#endif
#ifdef SUPPORT_AOT_XE2
    {"xe2", XE2_FAMILY},
#endif
#ifdef SUPPORT_AOT_XE3
    {"xe3", XE3_FAMILY},
#endif
};

inline const std::map<std::string, RELEASE> releaseAcronyms = {
#ifdef SUPPORT_AOT_XE_LP
    {"xe-lp", XE_LP_RELEASE},
    {"gen12lp", XE_LP_RELEASE},
#endif
#ifdef SUPPORT_AOT_XE_HP
    {"xe-hp", XE_HP_RELEASE},
#endif
#ifdef SUPPORT_AOT_XE_HPG
    {"xe-hpg", XE_HPG_RELEASE},
#endif
#ifdef SUPPORT_AOT_XE_HPC
    {"xe-hpc", XE_HPC_RELEASE},
#endif
#ifdef SUPPORT_AOT_XE_HPC_VG
    {"xe-hpc-vg", XE_HPC_VG_RELEASE},
#endif
#ifdef SUPPORT_AOT_XE_LPG
    {"xe-lpg", XE_LPG_RELEASE},
#endif
#ifdef SUPPORT_AOT_XE_LPGPLUS
    {"xe-lpgplus", XE_LPGPLUS_RELEASE},
#endif
#ifdef SUPPORT_AOT_XE2_HPG
    {"xe2-hpg", XE2_HPG_RELEASE},
#endif
#ifdef SUPPORT_AOT_XE2_LPG
    {"xe2-lpg", XE2_LPG_RELEASE},
#endif
#ifdef SUPPORT_AOT_XE3_LPG
    {"xe3-lpg", XE3_LPG_RELEASE},
#endif
};

inline const std::map<std::string, PRODUCT_CONFIG> deviceAcronyms = {
#ifdef SUPPORT_AOT_TGLLP
    {"tgllp", TGL},
    {"tgl", TGL},
#endif
#ifdef SUPPORT_AOT_RKLC
    {"rkl", RKL},
#endif
#ifdef SUPPORT_AOT_ADLS
    {"adl-s", ADL_S},
    {"rpl-s", ADL_S},
#endif
#ifdef SUPPORT_AOT_ADL
    {"adl-p", ADL_P},
    {"rpl-p", ADL_P},
    {"adl-n", ADL_N},
#endif
#ifdef SUPPORT_AOT_DG1
    {"dg1", DG1},
#endif
#ifdef SUPPORT_AOT_DG2
    {"acm-g10", DG2_G10_C0},
    {"dg2-g10", DG2_G10_C0},
    {"ats-m150", DG2_G10_C0},
    {"acm-g11", DG2_G11_B1},
    {"dg2-g11", DG2_G11_B1},
    {"ats-m75", DG2_G11_B1},
    {"acm-g12", DG2_G12_A0},
    {"dg2-g12", DG2_G12_A0},
#endif
#ifdef SUPPORT_AOT_PVC
    {"pvc-sdv", PVC_XL_A0P},
    {"pvc", PVC_XT_C0},
    {"pvc-vg", PVC_XT_C0_VG},
#endif
#ifdef SUPPORT_AOT_MTL
    {"mtl-u", MTL_U_B0},
    {"mtl-s", MTL_U_B0},
    {"mtl-m", MTL_U_B0},
    {"arl-u", MTL_U_B0},
    {"arl-s", MTL_U_B0},
    {"mtl-h", MTL_H_B0},
    {"mtl-p", MTL_H_B0},
#endif
#ifdef SUPPORT_AOT_ARL
    {"arl-h", ARL_H_B0},
#endif
#ifdef SUPPORT_AOT_BMG
    {"bmg-g21", BMG_G21_A0},
    {"bmg-g31", BMG_G31_A0},
#endif
#ifdef SUPPORT_AOT_LNL
    {"lnl-m", LNL_B0},
#endif
#ifdef SUPPORT_AOT_PTL
    {"ptl-h", PTL_H_B0},
    {"ptl-u", PTL_U_A0},
#endif
#ifdef SUPPORT_AOT_WCL
    {"wcl", WCL_A1},
#endif
};

inline const std::map<std::string, PRODUCT_CONFIG> rtlIdAcronyms = {
#ifdef SUPPORT_AOT_DG2
#ifdef SUPPORT_AOT_XE_HPG
    {"dg2-g10-a0", DG2_G10_A0},
    {"dg2-g10-a1", DG2_G10_A1},
    {"dg2-g10-b0", DG2_G10_B0},
    {"dg2-g10-c0", DG2_G10_C0},
    {"dg2-g11-a0", DG2_G11_A0},
    {"dg2-g11-b0", DG2_G11_B0},
    {"dg2-g11-b1", DG2_G11_B1},
    {"dg2-g12-a0", DG2_G12_A0},
#endif
#endif
#ifdef SUPPORT_AOT_PVC
#ifdef SUPPORT_AOT_XE_HPC
    {"pvc-xl-a0", PVC_XL_A0},
    {"pvc-xl-a0p", PVC_XL_A0P},
    {"pvc-xt-a0", PVC_XT_A0},
    {"pvc-xt-b0", PVC_XT_B0},
    {"pvc-xt-b1", PVC_XT_B1},
    {"pvc-xt-c0", PVC_XT_C0},
    {"pvc-xt-c0-vg", PVC_XT_C0_VG},
#endif
#endif
#ifdef SUPPORT_AOT_MTL
#ifdef SUPPORT_AOT_XE_LPG
    {"mtl-u-a0", MTL_U_A0},
    {"mtl-u-b0", MTL_U_B0},
    {"mtl-h-a0", MTL_H_A0},
    {"mtl-h-b0", MTL_H_B0},
#endif
#endif
#ifdef SUPPORT_AOT_ARL
#ifdef SUPPORT_AOT_XE_LPGPLUS
    {"arl-h-a0", ARL_H_A0},
    {"arl-h-b0", ARL_H_B0},
#endif
#endif
#ifdef SUPPORT_AOT_BMG
#ifdef SUPPORT_AOT_XE2_HPG
    {"bmg-g21-a0", BMG_G21_A0},
    {"bmg-g21-a1", BMG_G21_A0},
    {"bmg-g21-b0", BMG_G21_A0},
    {"bmg-g31-a0", BMG_G31_A0},
#endif
#endif
#ifdef SUPPORT_AOT_LNL
#ifdef SUPPORT_AOT_XE2_LPG
    {"lnl-a0", LNL_A0},
    {"lnl-a1", LNL_A1},
    {"lnl-b0", LNL_B0},
#endif
#endif
#ifdef SUPPORT_AOT_PTL
#ifdef SUPPORT_AOT_XE3_LPG
    {"ptl-h-a0", PTL_H_A0},
    {"ptl-h-b0", PTL_H_B0},
    {"ptl-u-a0", PTL_U_A0},
    {"ptl-u-a1", PTL_U_A1},
#endif
#endif
#ifdef SUPPORT_AOT_WCL
#ifdef SUPPORT_AOT_XE3_LPG
    {"wcl-a0", WCL_A0},
    {"wcl-a1", WCL_A1},
#endif
#endif
};

inline const std::map<std::string, PRODUCT_CONFIG> genericIdAcronyms = {
#ifdef SUPPORT_AOT_DG2
    {"dg2", DG2_G10_C0},
#endif
#ifdef SUPPORT_AOT_MTL
    {"mtl", MTL_U_B0},
#endif
#ifdef SUPPORT_AOT_BMG
    {"bmg", BMG_G21_A0},
#endif
#ifdef SUPPORT_AOT_PTL
    {"ptl", PTL_H_B0},
#endif
};

inline const std::map<PRODUCT_CONFIG, std::vector<PRODUCT_CONFIG>> compatibilityMapping = {
    {DG2_G10_C0, {DG2_G11_B1, DG2_G12_A0}},
    {MTL_U_B0, {MTL_H_B0}},
    {BMG_G21_A0, {BMG_G31_A0, LNL_B0}},
    {BMG_G21_A1_RESERVED, {BMG_G21_A0, BMG_G31_A0, LNL_B0}},
    {BMG_G21_B0_RESERVED, {BMG_G21_A0, BMG_G31_A0, LNL_B0}},
    {PTL_H_B0, {PTL_U_A0, WCL_A0}},
};
} // namespace AOT
