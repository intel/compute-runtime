/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
void HwInfoConfigHw<gfxProduct>::getKernelExtendedProperties(uint32_t *fp16, uint32_t *fp32, uint32_t *fp64) {
    *fp16 = 0u;
    *fp32 = FP_ATOMIC_EXT_FLAG_GLOBAL_ADD;
    *fp64 = 0u;
}
