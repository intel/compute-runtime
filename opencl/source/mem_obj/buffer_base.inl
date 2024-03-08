/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/populate_factory.h"

#include "opencl/source/mem_obj/buffer.h"

#include "encode_surface_state_args.h"

namespace NEO {

template <typename GfxFamily>
void BufferHw<GfxFamily>::setArgStateful(void *memory, bool forceNonAuxMode, bool disableL3, bool alignSizeForAuxTranslation,
                                         bool isReadOnlyArgument, const Device &device, bool areMultipleSubDevicesInContext) {
    auto rootDeviceIndex = device.getRootDeviceIndex();
    auto graphicsAllocation = multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex);
    const auto isReadOnly = isValueSet(getFlags(), CL_MEM_READ_ONLY) || isReadOnlyArgument;
    auto isDebuggerActive = device.getDebugger() != nullptr;

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
    args.areMultipleSubDevicesInContext = areMultipleSubDevicesInContext;
    args.implicitScaling = ImplicitScalingHelper::isImplicitScalingEnabled(device.getDeviceBitfield(), true);
    args.isDebuggerActive = isDebuggerActive;
    appendSurfaceStateArgs(args);
    EncodeSurfaceState<GfxFamily>::encodeBuffer(args);
}
} // namespace NEO
