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
    {"gen8", GEN8_FAMILY},
    {"gen9", GEN9_FAMILY},
    {"gen11", GEN11_FAMILY},
    {"gen12lp", GEN12LP_FAMILY},
    {"xe", XE_FAMILY},
};

static const std::map<std::string, RELEASE> releaseAcronyms = {
    {"gen8", GEN8_RELEASE},
    {"gen9", GEN9_RELEASE},
    {"gen11", GEN11_RELEASE},
    {"gen12lp", GEN12LP_RELEASE},
    {"xe-hp", XE_HP_RELEASE},
    {"xe-hpg", XE_HPG_RELEASE},
    {"xe-hpc", XE_HPC_RELEASE},
};

static const std::map<std::string, AOT::PRODUCT_CONFIG> productConfigAcronyms = {
    {"bdw", BDW},
    {"skl", SKL},
    {"kbl", KBL},
    {"cfl", CFL},
    {"apl", APL},
    {"bxt", APL},
    {"glk", GLK},
    {"whl", WHL},
    {"aml", AML},
    {"cml", CML},
    {"icllp", ICL},
    {"lkf", LKF},
    {"ehl", EHL},
    {"jsl", EHL},
    {"tgllp", TGL},
    {"rkl", RKL},
    {"rpl-s", RPL_S},
    {"adl-s", ADL_S},
    {"adl-p", ADL_P},
    {"adl-n", ADL_N},
    {"dg1", DG1},
    {"acm-g10", DG2_G10_C0},
    {"dg2-g10", DG2_G10_C0},
    {"ats-m150", DG2_G10_C0},
    {"acm-g11", DG2_G11_B1},
    {"dg2-g11", DG2_G11_B1},
    {"ats-m75", DG2_G11_B1},
    {"acm-g12", DG2_G12_A0},
    {"dg2-g12", DG2_G12_A0},
    {"pvc-sdv", PVC_XL_A0P},
    {"pvc", PVC_XT_C0},
};
} // namespace AOT
