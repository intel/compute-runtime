/*
 * Copyright (C) 2019-2020 Intel Corporation
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
        return IGA_GEN9p5;
    case IGFX_COFFEELAKE:
        return IGA_GEN9p5;
    case IGFX_ICELAKE:
        return IGA_GEN11;
    case IGFX_ICELAKE_LP:
        return IGA_GEN11;
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
        return IGA_GEN11;
    case IGFX_GEN11LP_CORE:
        return IGA_GEN11;
    }
}

iga_gen_t translateToIgaGen(PRODUCT_FAMILY productFamily);
iga_gen_t translateToIgaGen(GFXCORE_FAMILY coreFamily);
