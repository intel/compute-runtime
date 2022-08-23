/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/kernel/implicit_args.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/core/source/module/module.h"

#include "igfxfmid.h"

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
struct KernelHw : public KernelImp {
    using KernelImp::KernelImp;
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    void setBufferSurfaceState(uint32_t argIndex, void *address, NEO::GraphicsAllocation *alloc) override {
        uint64_t baseAddress = alloc->getGpuAddressToPatch();
        auto sshAlignmentMask = NEO::EncodeSurfaceState<GfxFamily>::getSurfaceBaseAddressAlignmentMask();

        // Remove misaligned bytes, accounted for in bufferOffset patch token
        baseAddress &= sshAlignmentMask;
        auto misalignedSize = ptrDiff(alloc->getGpuAddressToPatch(), baseAddress);
        auto offset = ptrDiff(address, reinterpret_cast<void *>(baseAddress));
        size_t bufferSizeForSsh = alloc->getUnderlyingBufferSize();
        auto argInfo = kernelImmData->getDescriptor().payloadMappings.explicitArgs[argIndex].as<NEO::ArgDescPointer>();
        bool offsetWasPatched = NEO::patchNonPointer<uint32_t, uint32_t>(ArrayRef<uint8_t>(this->crossThreadData.get(), this->crossThreadDataSize),
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
        // Allocation MUST be cacheline (64 byte) aligned in order to enable L3 caching otherwise Heap corruption will occur coming from the KMD.
        // Most commonly this issue will occur with Host Point Allocations from customers.
        l3Enabled = isL3Capable(*alloc);

        Device *device = module->getDevice();
        NEO::Device *neoDevice = device->getNEODevice();

        auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(reinterpret_cast<void *>(alloc->getGpuAddress()));
        if (allocData && allocData->allocationFlagsProperty.flags.locallyUncachedResource) {
            l3Enabled = false;
        }

        if (l3Enabled == false) {
            this->kernelRequiresQueueUncachedMocsCount++;
        }
        auto isDebuggerActive = neoDevice->isDebuggerActive() || neoDevice->getDebugger() != nullptr;
        NEO::EncodeSurfaceStateArgs args;
        args.outMemory = &surfaceState;
        args.graphicsAddress = bufferAddressForSsh;
        args.size = bufferSizeForSsh;
        args.mocs = device->getMOCS(l3Enabled, false);
        args.numAvailableDevices = neoDevice->getNumGenericSubDevices();
        args.allocation = alloc;
        args.gmmHelper = neoDevice->getGmmHelper();
        args.useGlobalAtomics = kernelImmData->getDescriptor().kernelAttributes.flags.useGlobalAtomics;
        args.areMultipleSubDevicesInContext = args.numAvailableDevices > 1;
        args.implicitScaling = device->isImplicitScalingCapable();
        args.isDebuggerActive = isDebuggerActive;

        NEO::EncodeSurfaceState<GfxFamily>::encodeBuffer(args);
        *reinterpret_cast<typename GfxFamily::RENDER_SURFACE_STATE *>(surfaceStateAddress) = surfaceState;
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
