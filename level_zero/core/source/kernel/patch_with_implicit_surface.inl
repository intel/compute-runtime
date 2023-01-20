/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

inline void patchWithImplicitSurface(ArrayRef<uint8_t> crossThreadData, ArrayRef<uint8_t> surfaceStateHeap,
                                     uintptr_t ptrToPatchInCrossThreadData, NEO::GraphicsAllocation &allocation,
                                     const NEO::ArgDescPointer &ptr, const NEO::Device &device, bool useGlobalAtomics,
                                     bool implicitScaling) {
    if (false == crossThreadData.empty()) {
        NEO::patchPointer(crossThreadData, ptr, ptrToPatchInCrossThreadData);
    }

    if ((false == surfaceStateHeap.empty()) && (NEO::isValidOffset(ptr.bindful))) {
        auto surfaceState = surfaceStateHeap.begin() + ptr.bindful;
        auto addressToPatch = allocation.getGpuAddress();
        size_t sizeToPatch = allocation.getUnderlyingBufferSize();

        auto &gfxCoreHelper = device.getGfxCoreHelper();
        auto isDebuggerActive = device.isDebuggerActive() || device.getDebugger() != nullptr;
        NEO::EncodeSurfaceStateArgs args;
        args.outMemory = surfaceState;
        args.size = sizeToPatch;
        args.graphicsAddress = addressToPatch;
        args.gmmHelper = device.getGmmHelper();
        args.allocation = &allocation;
        args.useGlobalAtomics = useGlobalAtomics;
        args.numAvailableDevices = device.getNumGenericSubDevices();
        args.areMultipleSubDevicesInContext = args.numAvailableDevices > 1;
        args.mocs = gfxCoreHelper.getMocsIndex(*args.gmmHelper, true, false) << 1;
        args.implicitScaling = implicitScaling;
        args.isDebuggerActive = isDebuggerActive;

        gfxCoreHelper.encodeBufferSurfaceState(args);
    }
}
