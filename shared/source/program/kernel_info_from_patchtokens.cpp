/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/program/kernel_info_from_patchtokens.h"

#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/kernel/kernel_descriptor_from_patchtokens.h"
#include "shared/source/program/kernel_info.h"

#include <cstring>

namespace NEO {

using namespace iOpenCL;

void populateKernelInfo(KernelInfo &dst, const PatchTokenBinary::KernelFromPatchtokens &src, uint32_t gpuPointerSizeInBytes) {
    UNRECOVERABLE_IF(nullptr == src.header);

    dst.heapInfo.DynamicStateHeapSize = src.header->DynamicStateHeapSize;
    dst.heapInfo.GeneralStateHeapSize = src.header->GeneralStateHeapSize;
    dst.heapInfo.SurfaceStateHeapSize = src.header->SurfaceStateHeapSize;
    dst.heapInfo.KernelHeapSize = src.header->KernelHeapSize;
    dst.heapInfo.KernelUnpaddedSize = src.header->KernelUnpaddedSize;
    dst.shaderHashCode = src.header->ShaderHashCode;

    dst.heapInfo.pKernelHeap = src.isa.begin();
    dst.heapInfo.pGsh = src.heaps.generalState.begin();
    dst.heapInfo.pDsh = src.heaps.dynamicState.begin();
    dst.heapInfo.pSsh = src.heaps.surfaceState.begin();

    if (src.tokens.executionEnvironment != nullptr) {
        dst.kernelDescriptor.kernelAttributes.hasIndirectStatelessAccess = (src.tokens.executionEnvironment->IndirectStatelessCount > 0);
    }

    dst.systemKernelOffset = src.tokens.stateSip ? src.tokens.stateSip->SystemKernelOffset : 0U;

    if (src.tokens.gtpinInfo) {
        dst.igcInfoForGtpin = reinterpret_cast<const gtpin::igc_info_t *>(src.tokens.gtpinInfo + 1);
    }

    populateKernelDescriptor(dst.kernelDescriptor, src, gpuPointerSizeInBytes);

    if (dst.kernelDescriptor.kernelAttributes.crossThreadDataSize) {
        dst.crossThreadData = new char[dst.kernelDescriptor.kernelAttributes.crossThreadDataSize];
        memset(dst.crossThreadData, 0x00, dst.kernelDescriptor.kernelAttributes.crossThreadDataSize);
    }
}

} // namespace NEO
