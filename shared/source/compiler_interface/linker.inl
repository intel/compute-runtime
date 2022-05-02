/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/linker.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"

namespace NEO {

template <typename PatchSizeT>
void Linker::patchIncrement(Device *pDevice, GraphicsAllocation *dstAllocation, size_t relocationOffset, const void *initData, uint64_t incrementValue) {

    auto &hwInfo = pDevice->getHardwareInfo();
    auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);

    bool useBlitter = hwInfoConfig.isBlitCopyRequiredForLocalMemory(hwInfo, *dstAllocation);

    auto initValue = ptrOffset(initData, relocationOffset);

    PatchSizeT value = 0;
    memcpy_s(&value, sizeof(PatchSizeT), initValue, sizeof(PatchSizeT));
    value += static_cast<PatchSizeT>(incrementValue);

    MemoryTransferHelper::transferMemoryToAllocation(useBlitter, *pDevice, dstAllocation, relocationOffset, &value, sizeof(PatchSizeT));
}

} // namespace NEO
