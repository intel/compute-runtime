/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {
template <>
void ProductHelperHw<gfxProduct>::getKernelExtendedProperties(uint32_t *fp16, uint32_t *fp32, uint32_t *fp64) const {
    *fp16 = 0u;
    *fp32 = FP_ATOMIC_EXT_FLAG_GLOBAL_ADD;
    *fp64 = 0u;
}
} // namespace NEO
