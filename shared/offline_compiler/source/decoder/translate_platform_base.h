/*
 * Copyright (C) 2019-2022 Intel Corporation
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
    case IGFX_BROADWELL:
        return IGA_GEN8;
    case IGFX_CHERRYVIEW:
        return IGA_GEN8lp;
    case IGFX_SKYLAKE:
        return IGA_GEN9;
    case IGFX_BROXTON:
        return IGA_GEN9lp;
    case IGFX_KABYLAKE:
    case IGFX_COFFEELAKE:
        return IGA_GEN9p5;
    case IGFX_ICELAKE:
    case IGFX_ICELAKE_LP:
    case IGFX_LAKEFIELD:
    case IGFX_ELKHARTLAKE:
        return IGA_GEN11;
    case IGFX_TIGERLAKE_LP:
    case IGFX_ROCKETLAKE:
    case IGFX_ALDERLAKE_S:
    case IGFX_ALDERLAKE_P:
    case IGFX_ALDERLAKE_N:
    case IGFX_DG1:
        return IGA_XE;
    case IGFX_XE_HP_SDV:
        return IGA_XE_HP;
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
    case IGFX_GEN8_CORE:
        return IGA_GEN8;
    case IGFX_GEN9_CORE:
        return IGA_GEN9;
    case IGFX_GEN11_CORE:
    case IGFX_GEN11LP_CORE:
        return IGA_GEN11;
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
