/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/page_table_entry_bits.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/gen12lp/helpers_gen12lp.h"
#include "shared/source/helpers/hw_helper.h"

#include "opencl/source/command_stream/command_stream_receiver_simulated_common_hw.h"

namespace NEO {
namespace Gen12LPHelpers {

bool pipeControlWaRequired(PRODUCT_FAMILY productFamily) {
    return (productFamily == IGFX_TIGERLAKE_LP) || (productFamily == IGFX_DG1);
}

uint32_t getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) {
    if (hwInfo.platform.eProductFamily == PRODUCT_FAMILY::IGFX_DG1) {
        switch (stepping) {
        case REVISION_A0:
            return 0x0;
        case REVISION_B:
            return 0x1;
        }
    }
    return CommonConstants::invalidStepping;
}

uint32_t getSteppingFromHwRevId(uint32_t hwRevId, const HardwareInfo &hwInfo) {
    if (hwInfo.platform.eProductFamily == PRODUCT_FAMILY::IGFX_DG1) {
        switch (hwRevId) {
        case 0x0:
            return REVISION_A0;
        case 0x1:
            return REVISION_B;
        }
    }
    return CommonConstants::invalidStepping;
}

bool imagePitchAlignmentWaRequired(PRODUCT_FAMILY productFamily) {
    return (productFamily == IGFX_TIGERLAKE_LP) || (productFamily == IGFX_DG1);
}

void adjustCoherencyFlag(PRODUCT_FAMILY productFamily, bool &coherencyFlag) {
    if (productFamily == IGFX_DG1) {
        coherencyFlag = false;
    }
}

bool isLocalMemoryEnabled(const HardwareInfo &hwInfo) {
    return hwInfo.featureTable.ftrLocalMemory;
}

void initAdditionalGlobalMMIO(const CommandStreamReceiver &commandStreamReceiver, AubMemDump::AubStream &stream) {}

uint64_t getPPGTTAdditionalBits(GraphicsAllocation *graphicsAllocation) {
    if (graphicsAllocation && graphicsAllocation->getMemoryPool() == MemoryPool::LocalMemory) {
        return BIT(PageTableEntry::localMemoryBit);
    }
    return 0;
}

void adjustAubGTTData(const CommandStreamReceiver &commandStreamReceiver, AubGTTData &data) {
    data.localMemory = commandStreamReceiver.isLocalMemoryEnabled();
}

void setAdditionalPipelineSelectFields(void *pipelineSelectCmd,
                                       const PipelineSelectArgs &pipelineSelectArgs,
                                       const HardwareInfo &hwInfo) {}

bool isOffsetToSkipSetFFIDGPWARequired(const HardwareInfo &hwInfo) {
    HwHelper &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    return hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo);
}

bool isForceEmuInt32DivRemSPWARequired(const HardwareInfo &hwInfo) {
    HwHelper &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    return (((hwInfo.platform.eProductFamily == IGFX_TIGERLAKE_LP) & (hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo))) ||
            ((hwInfo.platform.eProductFamily == IGFX_DG1) & (hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo))) ||
            ((hwInfo.platform.eProductFamily == IGFX_ROCKETLAKE) & (hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_C, hwInfo))));
}

bool is3DPipelineSelectWARequired(const HardwareInfo &hwInfo) {
    return (hwInfo.platform.eProductFamily == IGFX_TIGERLAKE_LP || hwInfo.platform.eProductFamily == IGFX_DG1 || hwInfo.platform.eProductFamily == IGFX_ROCKETLAKE);
}

bool forceBlitterUseForGlobalBuffers(const HardwareInfo &hwInfo, GraphicsAllocation *allocation) {
    if (hwInfo.platform.eProductFamily == PRODUCT_FAMILY::IGFX_DG1 && allocation->getUnderlyingBuffer() == 0) {
        return true;
    }

    return false;
}

} // namespace Gen12LPHelpers
} // namespace NEO
