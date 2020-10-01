/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/bit_helpers.h"

#include "opencl/source/helpers/surface_formats.h"
#include "opencl/source/mem_obj/buffer.h"

#include "buffer_ext.inl"
#include "hw_cmds.h"

namespace NEO {

union SURFACE_STATE_BUFFER_LENGTH {
    uint32_t Length;
    struct SurfaceState {
        uint32_t Width : BITFIELD_RANGE(0, 6);
        uint32_t Height : BITFIELD_RANGE(7, 20);
        uint32_t Depth : BITFIELD_RANGE(21, 31);
    } SurfaceState;
};

template <typename GfxFamily>
void BufferHw<GfxFamily>::setArgStateful(void *memory, bool forceNonAuxMode, bool disableL3, bool alignSizeForAuxTranslation, bool isReadOnlyArgument, const Device &device) {
    auto rootDeviceIndex = device.getRootDeviceIndex();
    auto graphicsAllocation = multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex);
    const auto isReadOnly = isValueSet(getFlags(), CL_MEM_READ_ONLY) || isReadOnlyArgument;
    EncodeSurfaceState<GfxFamily>::encodeBuffer(memory, getBufferAddress(rootDeviceIndex),
                                                getSurfaceSize(alignSizeForAuxTranslation, rootDeviceIndex),
                                                getMocsValue(disableL3, isReadOnly, rootDeviceIndex),
                                                true, forceNonAuxMode, isReadOnly, device.getNumAvailableDevices(),
                                                graphicsAllocation, device.getGmmHelper());
    appendSurfaceStateExt(memory);
}
} // namespace NEO
