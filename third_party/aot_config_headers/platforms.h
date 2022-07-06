/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <map>
#include <string>

#pragma once

namespace AOT {

enum PRODUCT_CONFIG : uint32_t {
    UNKNOWN_ISA = 0,
    BDW = 0x02000000,
    SKL = 0x02400009,
    KBL = 0x02404009,
    CFL = 0x02408009,
    APL = 0x0240c000,
    GLK = 0x02410000,
    WHL = 0x02414000,
    AML = 0x02418000,
    CML = 0x0241c000,
    ICL = 0x02c00000,
    LKF = 0x02c04000,
    EHL = 0x02c08000,
    TGL = 0x03000000,
    RKL = 0x03004000,
    RPL_S = 0x03008000,
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
    CONFIG_MAX_PLATFORM,
};

enum RELEASE : uint32_t {
    UNKNOWN_RELEASE = 0,
    GEN8_RELEASE,
    GEN9_RELEASE,
    GEN11_RELEASE,
    GEN12LP_RELEASE,
    XE_HP_RELEASE,
    XE_HPG_RELEASE,
    XE_HPC_RELEASE,
    RELEASE_MAX,
};

enum FAMILY : uint32_t {
    UNKNOWN_FAMILY = 0,
    GEN8_FAMILY,
    GEN9_FAMILY,
    GEN11_FAMILY,
    GEN12LP_FAMILY,
    XE_FAMILY,
    FAMILY_MAX,
};

static const std::map<std::string, FAMILY> familyAcronyms = {
#ifdef SUPPORT_GEN8
    {"gen8", GEN8_FAMILY},
#endif
#ifdef SUPPORT_GEN9
    {"gen9", GEN9_FAMILY},
#endif
#ifdef SUPPORT_GEN11
    {"gen11", GEN11_FAMILY},
#endif
#ifdef SUPPORT_GEN12LP
    {"gen12lp", GEN12LP_FAMILY},
#endif
#if defined(SUPPORT_XE_HP_CORE) || defined(SUPPORT_XE_HPC_CORE) || defined(SUPPORT_XE_HPG_CORE)
    {"xe", XE_FAMILY},
#endif
};

static const std::map<std::string, RELEASE> releaseAcronyms = {
#ifdef SUPPORT_GEN8
    {"gen8", GEN8_RELEASE},
#endif
#ifdef SUPPORT_GEN9
    {"gen9", GEN9_RELEASE},
#endif
#ifdef SUPPORT_GEN11
    {"gen11", GEN11_RELEASE},
#endif
#ifdef SUPPORT_GEN12LP
    {"gen12lp", GEN12LP_RELEASE},
#endif
#ifdef SUPPORT_XE_HP_CORE
    {"xe-hp", XE_HP_RELEASE},
#endif
#ifdef SUPPORT_XE_HPG_CORE
    {"xe-hpg", XE_HPG_RELEASE},
#endif
#ifdef SUPPORT_XE_HPC_CORE
    {"xe-hpc", XE_HPC_RELEASE},
#endif
};

static const std::map<std::string, AOT::PRODUCT_CONFIG> productConfigAcronyms = {
#ifdef SUPPORT_BDW
    {"bdw", BDW},
#endif
#ifdef SUPPORT_SKL
    {"skl", SKL},
#endif
#ifdef SUPPORT_KBL
    {"kbl", KBL},
    {"aml", AML},
#endif
#ifdef SUPPORT_CFL
    {"cfl", CFL},
    {"cml", CML},
    {"whl", WHL},
#endif
#ifdef SUPPORT_BXT
    {"apl", APL},
    {"bxt", APL},
#endif
#ifdef SUPPORT_GLK
    {"glk", GLK},
#endif
#ifdef SUPPORT_ICLLP
    {"icllp", ICL},
#endif
#ifdef SUPPORT_LKF
    {"lkf", LKF},
#endif
#ifdef SUPPORT_EHL
    {"ehl", EHL},
    {"jsl", EHL},
#endif
#ifdef SUPPORT_TGLLP
    {"tgllp", TGL},
#endif
#ifdef SUPPORT_RKL
    {"rkl", RKL},
#endif
#ifdef SUPPORT_ADLS
    {"adl-s", ADL_S},
#endif
#ifdef SUPPORT_ADLP
    {"adl-p", ADL_P},
#endif
#ifdef SUPPORT_ADLN
    {"adl-n", ADL_N},
#endif
#ifdef SUPPORT_DG1
    {"dg1", DG1},
#endif
#ifdef SUPPORT_DG2
    {"acm-g10", DG2_G10_C0},
    {"dg2-g10", DG2_G10_C0},
    {"ats-m150", DG2_G10_C0},
    {"acm-g11", DG2_G11_B1},
    {"dg2-g11", DG2_G11_B1},
    {"ats-m75", DG2_G11_B1},
    {"acm-g12", DG2_G12_A0},
    {"dg2-g12", DG2_G12_A0},
#endif
#ifdef SUPPORT_PVC
    {"pvc-sdv", PVC_XL_A0P},
    {"pvc", PVC_XT_C0},
#endif
};
} // namespace AOT
