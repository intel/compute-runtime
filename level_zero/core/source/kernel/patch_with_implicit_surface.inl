/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

inline void patchWithImplicitSurface(ArrayRef<uint8_t> crossThreadData, ArrayRef<uint8_t> surfaceStateHeap,
                                     uintptr_t ptrToPatchInCrossThreadData, NEO::GraphicsAllocation &allocation, uint64_t addressToPatch, size_t sizeToPatch,
                                     const NEO::ArgDescPointer &ptr, const NEO::Device &device,
                                     bool implicitScaling) {
    if (false == crossThreadData.empty()) {
        NEO::patchPointer(crossThreadData, ptr, ptrToPatchInCrossThreadData);
    }

    if ((false == surfaceStateHeap.empty()) && (NEO::isValidOffset(ptr.bindful))) {
        auto surfaceState = surfaceStateHeap.begin() + ptr.bindful;

        auto &gfxCoreHelper = device.getGfxCoreHelper();
        auto isDebuggerActive = device.getDebugger() != nullptr;
        NEO::EncodeSurfaceStateArgs args;
        args.outMemory = surfaceState;
        args.size = sizeToPatch;
        args.graphicsAddress = addressToPatch;
        args.gmmHelper = device.getGmmHelper();
        args.allocation = &allocation;
        args.numAvailableDevices = device.getNumGenericSubDevices();
        args.areMultipleSubDevicesInContext = args.numAvailableDevices > 1;
        args.mocs = gfxCoreHelper.getMocsIndex(*args.gmmHelper, true, false) << 1;
        args.implicitScaling = implicitScaling;
        args.isDebuggerActive = isDebuggerActive;

        gfxCoreHelper.encodeBufferSurfaceState(args);
    }
}

inline void patchWithImplicitSurface(ArrayRef<uint8_t> crossThreadData, ArrayRef<uint8_t> surfaceStateHeap,
                                     uintptr_t ptrToPatchInCrossThreadData, NEO::GraphicsAllocation &allocation,
                                     const NEO::ArgDescPointer &ptr, const NEO::Device &device,
                                     bool implicitScaling) {
    patchWithImplicitSurface(crossThreadData, surfaceStateHeap,
                             ptrToPatchInCrossThreadData, allocation, allocation.getGpuAddress(), allocation.getUnderlyingBufferSize(),
                             ptr, device, implicitScaling);
}

inline void patchImplicitArgBindlessOffsetAndSetSurfaceState(ArrayRef<uint8_t> crossThreadData, ArrayRef<uint8_t> surfaceStateHeap, NEO::GraphicsAllocation *allocation,
                                                             const NEO::ArgDescPointer &ptr, const NEO::Device &device, bool implicitScaling,
                                                             const NEO::SurfaceStateInHeapInfo &ssInHeap, const NEO::KernelDescriptor &kernelDescriptor) {
    auto &gfxCoreHelper = device.getGfxCoreHelper();
    void *surfaceStateAddress = nullptr;
    auto surfaceStateSize = gfxCoreHelper.getRenderSurfaceStateSize();
    bool useTempBuffer = false;

    if (NEO::isValidOffset(ptr.bindless)) {
        if (device.getBindlessHeapsHelper()) {
            surfaceStateAddress = ssInHeap.ssPtr;

            auto patchLocation = ptrOffset(crossThreadData.begin(), ptr.bindless);
            auto patchValue = gfxCoreHelper.getBindlessSurfaceExtendedMessageDescriptorValue(static_cast<uint32_t>(ssInHeap.surfaceStateOffset));
            patchWithRequiredSize(const_cast<uint8_t *>(patchLocation), sizeof(patchValue), patchValue);
            useTempBuffer = true;
        } else {
            auto index = std::numeric_limits<uint32_t>::max();
            const auto &iter = kernelDescriptor.getBindlessOffsetToSurfaceState().find(ptr.bindless);
            if (iter != kernelDescriptor.getBindlessOffsetToSurfaceState().end()) {
                index = iter->second;
            }

            if (index < std::numeric_limits<uint32_t>::max()) {
                surfaceStateAddress = ptrOffset(surfaceStateHeap.begin(), index * surfaceStateSize);
            }
        }
    }

    if (surfaceStateAddress) {
        std::unique_ptr<uint64_t[]> surfaceState;

        if (useTempBuffer) {
            surfaceState = std::make_unique<uint64_t[]>(surfaceStateSize / sizeof(uint64_t));
        }

        auto addressToPatch = allocation->getGpuAddress();
        size_t sizeToPatch = allocation->getUnderlyingBufferSize();
        auto isDebuggerActive = device.getDebugger() != nullptr;

        NEO::EncodeSurfaceStateArgs args;
        args.outMemory = useTempBuffer ? surfaceState.get() : surfaceStateAddress;
        args.graphicsAddress = addressToPatch;
        args.size = sizeToPatch;
        args.mocs = gfxCoreHelper.getMocsIndex(*device.getGmmHelper(), true, false) << 1;
        args.numAvailableDevices = device.getNumGenericSubDevices();
        args.allocation = allocation;
        args.gmmHelper = device.getGmmHelper();
        args.areMultipleSubDevicesInContext = args.numAvailableDevices > 1;
        args.implicitScaling = implicitScaling;
        args.isDebuggerActive = isDebuggerActive;

        gfxCoreHelper.encodeBufferSurfaceState(args);

        if (useTempBuffer) {
            memcpy_s(surfaceStateAddress, surfaceStateSize,
                     surfaceState.get(), surfaceStateSize);
        }
    }
}
