/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/memory_manager/memory_constants.h"

#include "mock_gdi.h"

extern ADAPTER_INFO gAdapterInfo;

void InitGfxPartition() {
    gAdapterInfo.GfxPartition.Standard.Base = 0x0000800400000000;
    gAdapterInfo.GfxPartition.Standard.Limit = 0x0000eeffffffffff;
    gAdapterInfo.GfxPartition.Standard64KB.Base = 0x0000b80200000000;
    gAdapterInfo.GfxPartition.Standard64KB.Limit = 0x0000efffffffffff;
    gAdapterInfo.GfxPartition.SVM.Base = 0;
    gAdapterInfo.GfxPartition.SVM.Limit = MemoryConstants::maxSvmAddress;
    gAdapterInfo.GfxPartition.Heap32[0].Base = 0x0000800000000000;
    gAdapterInfo.GfxPartition.Heap32[0].Limit = 0x00008000ffffefff;
    gAdapterInfo.GfxPartition.Heap32[1].Base = 0x0000800100000000;
    gAdapterInfo.GfxPartition.Heap32[1].Limit = 0x00008001ffffefff;
    gAdapterInfo.GfxPartition.Heap32[2].Base = 0x0000800200000000;
    gAdapterInfo.GfxPartition.Heap32[2].Limit = 0x00008002ffffefff;
    gAdapterInfo.GfxPartition.Heap32[3].Base = 0x0000800300000000;
    gAdapterInfo.GfxPartition.Heap32[3].Limit = 0x00008003ffffefff;
}
