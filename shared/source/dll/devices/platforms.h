/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

typedef enum {
    UNKNOWN_ISA = 0,
    BDW = 0x080000,
    SKL = 0x090000,
    KBL = 0x090100,
    CML = 0x090200,
    AML = 0x090200,
    WHL = 0x090200,
    CFL = 0x090200,
    BXT = 0x090300,
    APL = 0x090300,
    GLK = 0x090400,
    ICL = 0x0b0000,
    LKF = 0x0b0100,
    JSL = 0x0b0200,
    EHL = 0x0b0200,
    DG1 = 0x0c0000,
    RPL_S = 0x0c0000,
    ADL_P = 0x0c0000,
    ADL_S = 0x0c0000,
    RKL = 0x0c0000,
    TGL = 0x0c0000,
    XEHP_SDV = 0x0c0100,
    DG2_G10_A0 = 0x0c0200,
    DG2_G11 = 0x0c0201,
    DG2_G10_B0 = 0x0c0201,
    DG2_G12 = 0x0c0202,
    PVC_XL_A0 = 0x0c0300,
    PVC_XL_B0 = 0x0c0301,
    PVC_XT_A0 = 0x0c0400,
    PVC_XT_B0 = 0x0c0401,
    CONFIG_MAX_PLATFORM,
} PRODUCT_CONFIG;