/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/decoder/translate_platform_base.h"

iga_gen_t translateToIgaGen(PRODUCT_FAMILY productFamily) {
    return translateToIgaGenBase(productFamily);
}
