/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/gen12lp/helpers_gen12lp.h"

#include "opencl/source/aub_mem_dump/page_table_entry_bits.h"
#include "opencl/source/command_stream/command_stream_receiver_simulated_common_hw.h"

namespace NEO {
namespace Gen12LPHelpers {

bool pipeControlWaRequired(PRODUCT_FAMILY productFamily) {
    return (productFamily == IGFX_TIGERLAKE_LP) || (productFamily == IGFX_DG1);
}

bool workaroundRequired(uint32_t lowestSteppingWithBug, uint32_t steppingWithFix, const HardwareInfo &hwInfo) {
    return false;
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
    return (hwInfo.platform.usRevId == REVISION_A0);
}

bool isForceDefaultRCSEngineWARequired(const HardwareInfo &hwInfo) {
    return ((hwInfo.platform.eProductFamily == IGFX_TIGERLAKE_LP) || (hwInfo.platform.eProductFamily == IGFX_DG1) & (hwInfo.platform.usRevId == REVISION_A0));
}

bool isForceEmuInt32DivRemSPWARequired(const HardwareInfo &hwInfo) {
    return ((hwInfo.platform.eProductFamily == IGFX_TIGERLAKE_LP) & (hwInfo.platform.usRevId == REVISION_A0));
}

bool is3DPipelineSelectWARequired(const HardwareInfo &hwInfo) {
    return (hwInfo.platform.eProductFamily == IGFX_TIGERLAKE_LP || hwInfo.platform.eProductFamily == IGFX_DG1);
}

} // namespace Gen12LPHelpers
} // namespace NEO
