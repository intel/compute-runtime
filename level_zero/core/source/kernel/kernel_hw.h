/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/string.h"

#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/core/source/module/module.h"

#include "igfxfmid.h"

#include <algorithm>

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
struct KernelHw : public KernelImp {
    using KernelImp::KernelImp;
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    void setBufferSurfaceState(uint32_t argIndex, void *address, NEO::GraphicsAllocation *alloc) override {
        uintptr_t baseAddress = static_cast<uintptr_t>(alloc->getGpuAddress());
        auto sshAlignmentMask = NEO::EncodeSurfaceState<GfxFamily>::getSurfaceBaseAddressAlignmentMask();

        // Remove misalligned bytes, accounted for in in bufferOffset patch token
        baseAddress &= sshAlignmentMask;

        auto offset = ptrDiff(address, reinterpret_cast<void *>(baseAddress));
        size_t sizeTillEndOfSurface = alloc->getUnderlyingBufferSize() - offset;
        auto argInfo = kernelImmData->getDescriptor().payloadMappings.explicitArgs[argIndex].as<NEO::ArgDescPointer>();
        bool offsetWasPatched = NEO::patchNonPointer(ArrayRef<uint8_t>(this->crossThreadData.get(), this->crossThreadDataSize),
                                                     argInfo.bufferOffset, static_cast<uint32_t>(offset));
        if (false == offsetWasPatched) {
            // fallback to handling offset in surface state
            baseAddress = reinterpret_cast<uintptr_t>(address);
            DEBUG_BREAK_IF(baseAddress != (baseAddress & sshAlignmentMask));
            offset = 0;
        }

        auto surfaceStateAddress = ptrOffset(surfaceStateHeapData.get(), argInfo.bindful);
        void *bufferAddressForSsh = reinterpret_cast<void *>(baseAddress);
        auto alignment = NEO::EncodeSurfaceState<GfxFamily>::getSurfaceBaseAddressAlignment();
        size_t bufferSizeForSsh = ptrDiff(reinterpret_cast<void *>(alloc->getGpuAddress()), bufferAddressForSsh);
        bufferSizeForSsh += sizeTillEndOfSurface; // take address alignment offset into account
        bufferSizeForSsh = alignUp(bufferSizeForSsh, alignment);

        auto mocs = this->module->getDevice()->getMOCS(true, false);
        bool requiresCoherency;
        NEO::EncodeSurfaceState<GfxFamily>::encodeBuffer(surfaceStateAddress,
                                                         bufferAddressForSsh, bufferSizeForSsh, mocs,
                                                         requiresCoherency = false);
    }

    std::unique_ptr<Kernel> clone() const override {
        std::unique_ptr<Kernel> ret{new KernelHw<gfxCoreFamily>};
        auto cloned = static_cast<KernelHw<gfxCoreFamily> *>(ret.get());

        cloned->kernelImmData = kernelImmData;
        cloned->module = module;
        cloned->kernelArgHandlers.assign(this->kernelArgHandlers.begin(), this->kernelArgHandlers.end());
        cloned->residencyContainer.assign(this->residencyContainer.begin(), this->residencyContainer.end());

        if (printfBuffer != nullptr) {
            const auto &it = std::find(cloned->residencyContainer.rbegin(), cloned->residencyContainer.rend(), this->printfBuffer);
            if (it == cloned->residencyContainer.rbegin()) {
                cloned->residencyContainer.resize(cloned->residencyContainer.size() - 1);
            } else {
                std::iter_swap(it, cloned->residencyContainer.rbegin());
            }
            cloned->createPrintfBuffer();
        }

        std::copy(this->groupSize, this->groupSize + 3, cloned->groupSize);
        cloned->threadsPerThreadGroup = this->threadsPerThreadGroup;
        cloned->threadExecutionMask = this->threadExecutionMask;

        if (this->surfaceStateHeapDataSize > 0) {
            cloned->surfaceStateHeapData.reset(new uint8_t[this->surfaceStateHeapDataSize]);
            memcpy_s(cloned->surfaceStateHeapData.get(),
                     this->surfaceStateHeapDataSize,
                     this->surfaceStateHeapData.get(), this->surfaceStateHeapDataSize);
            cloned->surfaceStateHeapDataSize = this->surfaceStateHeapDataSize;
        }

        if (this->crossThreadDataSize != 0) {
            cloned->crossThreadData.reset(new uint8_t[this->crossThreadDataSize]);
            memcpy_s(cloned->crossThreadData.get(),
                     this->crossThreadDataSize,
                     this->crossThreadData.get(),
                     this->crossThreadDataSize);
            cloned->crossThreadDataSize = this->crossThreadDataSize;
        }

        if (this->dynamicStateHeapDataSize != 0) {
            cloned->dynamicStateHeapData.reset(new uint8_t[this->dynamicStateHeapDataSize]);
            memcpy_s(cloned->dynamicStateHeapData.get(),
                     this->dynamicStateHeapDataSize,
                     this->dynamicStateHeapData.get(), this->dynamicStateHeapDataSize);
            cloned->dynamicStateHeapDataSize = this->dynamicStateHeapDataSize;
        }

        if (this->perThreadDataForWholeThreadGroup != nullptr) {
            alignedFree(cloned->perThreadDataForWholeThreadGroup);
            cloned->perThreadDataForWholeThreadGroup = reinterpret_cast<uint8_t *>(alignedMalloc(perThreadDataSizeForWholeThreadGroupAllocated, 32));
            memcpy_s(cloned->perThreadDataForWholeThreadGroup,
                     this->perThreadDataSizeForWholeThreadGroupAllocated,
                     this->perThreadDataForWholeThreadGroup,
                     this->perThreadDataSizeForWholeThreadGroupAllocated);
            cloned->perThreadDataSizeForWholeThreadGroupAllocated = this->perThreadDataSizeForWholeThreadGroupAllocated;
            cloned->perThreadDataSizeForWholeThreadGroup = this->perThreadDataSizeForWholeThreadGroup;
            cloned->perThreadDataSize = this->perThreadDataSize;
        }

        return ret;
    }
};

} // namespace L0
