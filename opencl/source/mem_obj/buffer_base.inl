/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/populate_factory.h"

#include "opencl/source/mem_obj/buffer.h"

namespace NEO {

union SURFACE_STATE_BUFFER_LENGTH {
    uint32_t Length;
    struct SurfaceState {
        uint32_t Width : 7;
        uint32_t Height : 14;
        uint32_t Depth : 11;
    } SurfaceState;
};

template <typename GfxFamily>
void BufferHw<GfxFamily>::setArgStateful(void *memory, bool forceNonAuxMode, bool disableL3, bool alignSizeForAuxTranslation,
                                         bool isReadOnlyArgument, const Device &device, bool useGlobalAtomics, bool areMultipleSubDevicesInContext) {
    auto rootDeviceIndex = device.getRootDeviceIndex();
    auto graphicsAllocation = multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex);
    const auto isReadOnly = isValueSet(getFlags(), CL_MEM_READ_ONLY) || isReadOnlyArgument;
    auto isDebuggerActive = device.isDebuggerActive() || device.getDebugger() != nullptr;

    NEO::EncodeSurfaceStateArgs args;
    args.outMemory = memory;
    args.graphicsAddress = getBufferAddress(rootDeviceIndex);
    args.size = getSurfaceSize(alignSizeForAuxTranslation, rootDeviceIndex);
    args.mocs = getMocsValue(disableL3, isReadOnly, rootDeviceIndex);
    args.cpuCoherent = true;
    args.forceNonAuxMode = forceNonAuxMode;
    args.isReadOnly = isReadOnly;
    args.numAvailableDevices = device.getNumGenericSubDevices();
    args.allocation = graphicsAllocation;
    args.gmmHelper = device.getGmmHelper();
    args.useGlobalAtomics = useGlobalAtomics;
    args.areMultipleSubDevicesInContext = areMultipleSubDevicesInContext;
    args.implicitScaling = ImplicitScalingHelper::isImplicitScalingEnabled(device.getDeviceBitfield(), true);
    args.isDebuggerActive = isDebuggerActive;
    appendSurfaceStateArgs(args);
    EncodeSurfaceState<GfxFamily>::encodeBuffer(args);
}
} // namespace NEO
