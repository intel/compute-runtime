/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"

#include "mock_gdi.h"

extern ADAPTER_INFO_KMD gAdapterInfo;
extern uint64_t gGpuAddressSpace;

void initGfxPartition() {
    if (gGpuAddressSpace >= maxNBitValue(48)) {
        // Full-range SVM 48 bit
        gAdapterInfo.GfxPartition.Standard.Base = 0x0000800400000000;
        gAdapterInfo.GfxPartition.Standard.Limit = 0x0000b801ffffffff;
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
    } else if (gGpuAddressSpace == maxNBitValue(47)) {
        // Full-range SVM 47 bit
        gAdapterInfo.GfxPartition.Standard.Base = 0x000001c98a320000;
        gAdapterInfo.GfxPartition.Standard.Limit = 0x000005c98a31ffff;
        gAdapterInfo.GfxPartition.Standard64KB.Base = 0x000005ce00000000;
        gAdapterInfo.GfxPartition.Standard64KB.Limit = 0x000009cdffffffff;
        gAdapterInfo.GfxPartition.SVM.Base = 0;
        gAdapterInfo.GfxPartition.SVM.Limit = MemoryConstants::maxSvmAddress;
        gAdapterInfo.GfxPartition.Heap32[0].Base = 0x000005ca00000000;
        gAdapterInfo.GfxPartition.Heap32[0].Limit = 0x000005caffffefff;
        gAdapterInfo.GfxPartition.Heap32[1].Base = 0x000005cb00000000;
        gAdapterInfo.GfxPartition.Heap32[1].Limit = 0x000005cbffffefff;
        gAdapterInfo.GfxPartition.Heap32[2].Base = 0x000005cc00000000;
        gAdapterInfo.GfxPartition.Heap32[2].Limit = 0x000005ccffffefff;
        gAdapterInfo.GfxPartition.Heap32[3].Base = 0x000005cd00000000;
        gAdapterInfo.GfxPartition.Heap32[3].Limit = 0x000005cdffffefff;
    } else {
        // Limited range
        gAdapterInfo.GfxPartition.Standard.Base = 0x0000000100000000;
        gAdapterInfo.GfxPartition.Standard.Limit = 0x0000000FFFFFFFFF;
        gAdapterInfo.GfxPartition.Standard64KB.Base = 0x0000000100000000;
        gAdapterInfo.GfxPartition.Standard64KB.Limit = 0x0000000FFFFFFFFF;
        gAdapterInfo.GfxPartition.SVM.Base = 0x0;
        gAdapterInfo.GfxPartition.SVM.Limit = 0x0;
        gAdapterInfo.GfxPartition.Heap32[0].Base = 0x00001000;
        gAdapterInfo.GfxPartition.Heap32[0].Limit = 0xFFFFEFFF;
        gAdapterInfo.GfxPartition.Heap32[1].Base = 0x00001000;
        gAdapterInfo.GfxPartition.Heap32[1].Limit = 0xFFFFEFFF;
        gAdapterInfo.GfxPartition.Heap32[2].Base = 0x00001000;
        gAdapterInfo.GfxPartition.Heap32[2].Limit = 0xFFFFEFFF;
        gAdapterInfo.GfxPartition.Heap32[3].Base = 0x00001000;
        gAdapterInfo.GfxPartition.Heap32[3].Limit = 0xFFFFEFFF;
    }
}
