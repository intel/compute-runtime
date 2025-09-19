/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/core/source/kernel/kernel_shared_state.h"
#include "level_zero/core/source/module/module.h"

#include "encode_surface_state_args.h"
#include "implicit_args.h"
#include "neo_igfxfmid.h"

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
struct KernelHw : public KernelImp {
    using KernelImp::KernelImp;
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    void setBufferSurfaceState(uint32_t argIndex, void *address, NEO::GraphicsAllocation *alloc) override {
        uint64_t baseAddress = alloc->getGpuAddressToPatch();

        // Remove misaligned bytes, accounted for in bufferOffset patch token
        baseAddress &= this->sharedState->surfaceStateAlignmentMask;
        auto misalignedSize = ptrDiff(alloc->getGpuAddressToPatch(), baseAddress);
        auto offset = ptrDiff(address, reinterpret_cast<void *>(baseAddress));
        size_t bufferSizeForSsh = alloc->getUnderlyingBufferSize();
        // If the allocation is part of a mapped virtual range, then set size to maximum to allow for access across multiple virtual ranges.
        Device *device = module->getDevice();
        auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(reinterpret_cast<void *>(alloc->getGpuAddress()));

        auto argInfo = sharedState->kernelImmData->getDescriptor().payloadMappings.explicitArgs[argIndex].as<NEO::ArgDescPointer>();
        bool offsetWasPatched = NEO::patchNonPointer<uint32_t, uint32_t>(getCrossThreadDataSpan(),
                                                                         argInfo.bufferOffset, static_cast<uint32_t>(offset));
        bool offsetAddress = false;
        if (false == offsetWasPatched) {
            // fallback to handling offset in surface state
            offsetAddress = baseAddress != reinterpret_cast<uintptr_t>(address);
            baseAddress = reinterpret_cast<uintptr_t>(address);
            bufferSizeForSsh -= offset;
            DEBUG_BREAK_IF(baseAddress != (baseAddress & this->sharedState->surfaceStateAlignmentMask));

            offset = 0;
        }
        void *surfaceStateAddress = nullptr;
        auto surfaceState = GfxFamily::cmdInitRenderSurfaceState;

        if (NEO::isValidOffset(argInfo.bindful)) {
            surfaceStateAddress = &getSurfaceStateHeapDataSpan()[argInfo.bindful];
            surfaceState = *reinterpret_cast<typename GfxFamily::RENDER_SURFACE_STATE *>(surfaceStateAddress);

        } else if (NEO::isValidOffset(argInfo.bindless)) {
            privateState.isBindlessOffsetSet[argIndex] = false;
            privateState.usingSurfaceStateHeap[argIndex] = false;
            if (this->module->getDevice()->getNEODevice()->getBindlessHeapsHelper() && !offsetAddress) {
                surfaceStateAddress = patchBindlessSurfaceState(alloc, argInfo.bindless);
                privateState.isBindlessOffsetSet[argIndex] = true;
            } else {
                privateState.usingSurfaceStateHeap[argIndex] = true;
                const auto surfaceStateOffset = getSurfaceStateIndexForBindlessOffset(argInfo.bindless) * sizeof(typename GfxFamily::RENDER_SURFACE_STATE);
                surfaceStateAddress = &getSurfaceStateHeapDataSpan()[surfaceStateOffset];
            }
        }

        uint64_t bufferAddressForSsh = baseAddress;
        bufferSizeForSsh += misalignedSize;
        bufferSizeForSsh = alignUp(bufferSizeForSsh, this->sharedState->surfaceStateAlignment);

        bool l3Enabled = true;
        // Allocation MUST be cacheline (64 byte) aligned in order to enable L3 caching otherwise Heap corruption will occur coming from the KMD.
        // Most commonly this issue will occur with Host Point Allocations from customers.
        l3Enabled = isL3Capable(*alloc);

        NEO::Device *neoDevice = device->getNEODevice();

        if (allocData && allocData->allocationFlagsProperty.flags.locallyUncachedResource) {
            l3Enabled = false;
        }

        if (l3Enabled == false) {
            this->privateState.kernelRequiresQueueUncachedMocsCount++;
        }
        auto isDebuggerActive = neoDevice->getDebugger() != nullptr;
        NEO::EncodeSurfaceStateArgs args;
        args.outMemory = &surfaceState;
        args.graphicsAddress = bufferAddressForSsh;
        if (allocData && allocData->virtualReservationData) {
            bufferSizeForSsh = MemoryConstants::fullStatefulRegion;
        }
        args.size = bufferSizeForSsh;
        args.mocs = device->getMOCS(l3Enabled, false);
        args.numAvailableDevices = neoDevice->getNumGenericSubDevices();
        args.allocation = alloc;
        args.gmmHelper = neoDevice->getGmmHelper();
        args.areMultipleSubDevicesInContext = args.numAvailableDevices > 1;
        args.implicitScaling = device->isImplicitScalingCapable();
        args.isDebuggerActive = isDebuggerActive;

        NEO::EncodeSurfaceState<GfxFamily>::encodeBuffer(args);
        UNRECOVERABLE_IF(surfaceStateAddress == nullptr);
        *reinterpret_cast<typename GfxFamily::RENDER_SURFACE_STATE *>(surfaceStateAddress) = surfaceState;
    }
};

} // namespace L0
