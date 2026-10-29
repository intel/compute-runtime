/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {

struct HeapInfo {
    const void *pKernelHeap = nullptr;
    const void *pDsh = nullptr;
    const void *pSsh = nullptr;

    uint32_t kernelHeapSize = 0U;
    uint32_t dynamicStateHeapSize = 0U;
    uint32_t surfaceStateHeapSize = 0U;
};

} // namespace NEO
