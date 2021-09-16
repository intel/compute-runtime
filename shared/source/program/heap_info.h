/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {

struct HeapInfo {
    const void *pKernelHeap = nullptr;
    const void *pGsh = nullptr;
    const void *pDsh = nullptr;
    const void *pSsh = nullptr;

    uint32_t KernelHeapSize = 0U;
    uint32_t GeneralStateHeapSize = 0U;
    uint32_t DynamicStateHeapSize = 0U;
    uint32_t SurfaceStateHeapSize = 0U;
    uint32_t KernelUnpaddedSize = 0U;
};

} // namespace NEO
