/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/helpers/hw_helper.h"
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
        uint64_t baseAddress = alloc->getGpuAddressToPatch();
        auto sshAlignmentMask = NEO::EncodeSurfaceState<GfxFamily>::getSurfaceBaseAddressAlignmentMask();

        // Remove misalligned bytes, accounted for in in bufferOffset patch token
        baseAddress &= sshAlignmentMask;
        auto misalignedSize = ptrDiff(alloc->getGpuAddressToPatch(), baseAddress);
        auto offset = ptrDiff(address, reinterpret_cast<void *>(baseAddress));
        size_t bufferSizeForSsh = alloc->getUnderlyingBufferSize();
        auto argInfo = kernelImmData->getDescriptor().payloadMappings.explicitArgs[argIndex].as<NEO::ArgDescPointer>();
        bool offsetWasPatched = NEO::patchNonPointer(ArrayRef<uint8_t>(this->crossThreadData.get(), this->crossThreadDataSize),
                                                     argInfo.bufferOffset, static_cast<uint32_t>(offset));
        if (false == offsetWasPatched) {
            // fallback to handling offset in surface state
            baseAddress = reinterpret_cast<uintptr_t>(address);
            bufferSizeForSsh -= offset;
            DEBUG_BREAK_IF(baseAddress != (baseAddress & sshAlignmentMask));
            offset = 0;
        }
        void *surfaceStateAddress = nullptr;
        auto surfaceState = GfxFamily::cmdInitRenderSurfaceState;
        if (NEO::isValidOffset(argInfo.bindless)) {
            surfaceStateAddress = patchBindlessSurfaceState(alloc, argInfo.bindless);
        } else {
            surfaceStateAddress = ptrOffset(surfaceStateHeapData.get(), argInfo.bindful);
            surfaceState = *reinterpret_cast<typename GfxFamily::RENDER_SURFACE_STATE *>(surfaceStateAddress);
        }
        uint64_t bufferAddressForSsh = baseAddress;
        auto alignment = NEO::EncodeSurfaceState<GfxFamily>::getSurfaceBaseAddressAlignment();
        bufferSizeForSsh += misalignedSize;
        bufferSizeForSsh = alignUp(bufferSizeForSsh, alignment);

        bool l3Enabled = true;
        auto allocData = this->module->getDevice()->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(reinterpret_cast<void *>(alloc->getGpuAddress()));
        if (allocData && allocData->allocationFlagsProperty.flags.locallyUncachedResource) {
            l3Enabled = false;
        }
        auto mocs = this->module->getDevice()->getMOCS(l3Enabled, false);

        NEO::Device *neoDevice = module->getDevice()->getNEODevice();
        NEO::EncodeSurfaceState<GfxFamily>::encodeBuffer(&surfaceState, bufferAddressForSsh, bufferSizeForSsh, mocs,
                                                         false, false, false, neoDevice->getNumAvailableDevices(),
                                                         alloc, neoDevice->getGmmHelper(),
                                                         kernelImmData->getDescriptor().kernelAttributes.flags.useGlobalAtomics, 1u);
        *reinterpret_cast<typename GfxFamily::RENDER_SURFACE_STATE *>(surfaceStateAddress) = surfaceState;
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
        cloned->numThreadsPerThreadGroup = this->numThreadsPerThreadGroup;
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

    void evaluateIfRequiresGenerationOfLocalIdsByRuntime(const NEO::KernelDescriptor &kernelDescriptor) override {
        size_t localWorkSizes[3];
        localWorkSizes[0] = this->groupSize[0];
        localWorkSizes[1] = this->groupSize[1];
        localWorkSizes[2] = this->groupSize[2];

        kernelRequiresGenerationOfLocalIdsByRuntime = NEO::EncodeDispatchKernel<GfxFamily>::isRuntimeLocalIdsGenerationRequired(
            kernelDescriptor.kernelAttributes.numLocalIdChannels,
            localWorkSizes,
            std::array<uint8_t, 3>{
                {kernelDescriptor.kernelAttributes.workgroupWalkOrder[0],
                 kernelDescriptor.kernelAttributes.workgroupWalkOrder[1],
                 kernelDescriptor.kernelAttributes.workgroupWalkOrder[2]}},
            kernelDescriptor.kernelAttributes.flags.requiresWorkgroupWalkOrder,
            requiredWorkgroupOrder,
            kernelDescriptor.kernelAttributes.simdSize);
    }
};

} // namespace L0
