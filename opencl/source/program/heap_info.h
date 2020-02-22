/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "patch_info.h"

#include <cstdint>

namespace NEO {

struct HeapInfo {
    const SKernelBinaryHeaderCommon *pKernelHeader = nullptr;
    const void *pKernelHeap = nullptr;
    const void *pGsh = nullptr;
    const void *pDsh = nullptr;
    const void *pSsh = nullptr;
};

} // namespace NEO
