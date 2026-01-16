/*
 * Copyright (C) 2021-2026 Intel Corporation
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
    TGL = 0x03000000, // 12.0.0
    RKL = 0x03004000, // 12.1.0
    ADL_S = 0x03008000, // 12.2.0
    ADL_P = 0x0300c000, // 12.3.0
    ADL_N = 0x03010000, // 12.4.0
    DG1 = 0x03028000, // 12.10.0
    XEHP_SDV = 0x030c8004, // 12.50.4
    DG2_G10_A0 = 0x030dc000, // 12.55.0
    DG2_G10_A1 = 0x030dc001, // 12.55.1
    DG2_G10_B0 = 0x030dc004, // 12.55.4
    DG2_G10_C0 = 0x030dc008, // 12.55.8
    DG2_G11_A0 = 0x030e0000, // 12.56.0
    DG2_G11_B0 = 0x030e0004, // 12.56.4
    DG2_G11_B1 = 0x030e0005, // 12.56.5
    DG2_G12_A0 = 0x030e4000, // 12.57.0
    PVC_XL_A0 = 0x030f0000, // 12.60.0
    PVC_XL_A0P = 0x030f0001, // 12.60.1
    PVC_XT_A0 = 0x030f0003, // 12.60.3
    PVC_XT_B0 = 0x030f0005, // 12.60.5
    PVC_XT_B1 = 0x030f0006, // 12.60.6
    PVC_XT_C0 = 0x030f0007, // 12.60.7
    PVC_XT_C0_VG = 0x030f4007, // 12.61.7
    MTL_U_A0 = 0x03118000, // 12.70.0
    MTL_U_B0 = 0x03118004, // 12.70.4
    MTL_H_A0 = 0x0311c000, // 12.71.0
    MTL_H_B0 = 0x0311c004, // 12.71.4
    ARL_H_A0 = 0x03128000, // 12.74.0
    ARL_H_B0 = 0x03128004, // 12.74.4
    BMG_G21_A0 = 0x05004000, // 20.1.0
    BMG_G21_A1_RESERVED = 0x05004001, // 20.1.1
    BMG_G21_B0_RESERVED = 0x05004004, // 20.1.4
    BMG_G31_A0 = 0x05008000, // 20.2.0
    LNL_A0 = 0x05010000, // 20.4.0
    LNL_A1 = 0x05010001, // 20.4.1
    LNL_B0 = 0x05010004, // 20.4.4
    PTL_H_A0 = 0x07800000, // 30.0.0
    PTL_H_B0 = 0x07800004, // 30.0.4
    PTL_U_A0 = 0x07804000, // 30.1.0
    PTL_U_A1 = 0x07804001, // 30.1.1
    WCL_A0 = 0x0780c000, // 30.3.0
    WCL_A1 = 0x0780c001, // 30.3.1
    NVL_S_A0 = 0x07810000, // 30.4.0
    NVL_S_B0 = 0x07810004, // 30.4.4
    NVL_U_A0 = 0x07814000, // 30.5.0
    NVL_U_A1 = 0x07814001, // 30.5.1
    NVL_U_B0 = 0x07814004, // 30.5.4
    CRI_A0 = 0x08c2c000, // 35.11.0
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
    XE3P_XPC_RELEASE,
    RELEASE_MAX
};

enum FAMILY : uint32_t {
    UNKNOWN_FAMILY = 0,
    XE_FAMILY,
    XE2_FAMILY,
    XE3_FAMILY,
    XE3P_FAMILY,
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
#ifdef SUPPORT_AOT_XE3P
    {"xe3p", XE3P_FAMILY},
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
#ifdef SUPPORT_AOT_XE3P_XPC
    {"xe3p-xpc", XE3P_XPC_RELEASE},
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
#ifdef SUPPORT_AOT_NVL
    {"nvl-s", NVL_S_B0},
    {"nvl-hx", NVL_S_B0},
    {"nvl-ul", NVL_S_B0},
    {"nvl-u", NVL_U_B0},
    {"nvl-h", NVL_U_B0},
#endif
#ifdef SUPPORT_AOT_CRI
    {"cri", CRI_A0},
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
#ifdef SUPPORT_AOT_NVL
#ifdef SUPPORT_AOT_XE3_LPG
    {"nvl-s-a0", NVL_S_A0},
    {"nvl-s-b0", NVL_S_B0},
    {"nvl-u-a0", NVL_U_A0},
    {"nvl-u-a1", NVL_U_A1},
    {"nvl-u-b0", NVL_U_B0},
#endif
#endif
#ifdef SUPPORT_AOT_CRI
#ifdef SUPPORT_AOT_XE3P_XPC
    {"cri-a0", CRI_A0},
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
    {PTL_H_B0, {PTL_U_A0, WCL_A1, NVL_S_B0}},
};
} // namespace AOT
