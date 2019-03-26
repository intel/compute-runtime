/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "patch_info.h"

#include <cstdint>

namespace NEO {

struct HeapInfo {
    const SKernelBinaryHeaderCommon *pKernelHeader;
    const void *pKernelHeap;
    const void *pGsh;
    const void *pDsh;
    void *pSsh;
    const void *pPatchList;
    const void *pBlob;
    size_t blobSize;

    HeapInfo() {
        pKernelHeader = nullptr;
        pKernelHeap = nullptr;
        pGsh = nullptr;
        pDsh = nullptr;
        pSsh = nullptr;
        pPatchList = nullptr;
        pBlob = nullptr;
        blobSize = 0;
    }
};

} // namespace NEO
