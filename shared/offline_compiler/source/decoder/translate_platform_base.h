/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "igad.h"
#include "igfxfmid.h"

inline iga_gen_t translateToIgaGenBase(PRODUCT_FAMILY productFamily) {
    switch (productFamily) {
    default:
        return IGA_GEN_INVALID;
    case IGFX_TIGERLAKE_LP:
    case IGFX_ROCKETLAKE:
    case IGFX_ALDERLAKE_S:
    case IGFX_ALDERLAKE_P:
    case IGFX_ALDERLAKE_N:
    case IGFX_DG1:
        return IGA_XE;
    case IGFX_DG2:
        return IGA_XE_HPG;
    case IGFX_PVC:
        return IGA_XE_HPC;
    }
}

inline iga_gen_t translateToIgaGenBase(GFXCORE_FAMILY coreFamily) {
    switch (coreFamily) {
    default:
        return IGA_GEN_INVALID;
    case IGFX_GEN12LP_CORE:
        return IGA_XE;
    case IGFX_XE_HP_CORE:
        return IGA_XE_HP;
    case IGFX_XE_HPG_CORE:
        return IGA_XE_HPG;
    case IGFX_XE_HPC_CORE:
        return IGA_XE_HPC;
    }
}

iga_gen_t translateToIgaGen(PRODUCT_FAMILY productFamily);
iga_gen_t translateToIgaGen(GFXCORE_FAMILY coreFamily);
