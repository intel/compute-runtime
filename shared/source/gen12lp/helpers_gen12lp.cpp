/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/helpers_gen12lp.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/hw_helper.h"

#include "opencl/source/helpers/hardware_commands_helper.h"

namespace NEO {
namespace Gen12LPHelpers {

bool pipeControlWaRequired(PRODUCT_FAMILY productFamily) {
    return (productFamily == PRODUCT_FAMILY::IGFX_TIGERLAKE_LP);
}

bool workaroundRequired(uint32_t lowestSteppingWithBug, uint32_t steppingWithFix, const HardwareInfo &hwInfo) {
    return false;
}

bool imagePitchAlignmentWaRequired(PRODUCT_FAMILY productFamily) {
    return (productFamily == PRODUCT_FAMILY::IGFX_TIGERLAKE_LP);
}

void adjustCoherencyFlag(PRODUCT_FAMILY productFamily, bool &coherencyFlag) {}

bool isLocalMemoryEnabled(const HardwareInfo &hwInfo) {
    return false;
}

void initAdditionalGlobalMMIO(const CommandStreamReceiver &commandStreamReceiver, AubMemDump::AubStream &stream) {}

uint64_t getPPGTTAdditionalBits(GraphicsAllocation *graphicsAllocation) {
    return 0;
}

void adjustAubGTTData(const CommandStreamReceiver &commandStreamReceiver, AubGTTData &data) {}

void setAdditionalPipelineSelectFields(void *pipelineSelectCmd,
                                       const PipelineSelectArgs &pipelineSelectArgs,
                                       const HardwareInfo &hwInfo) {}

bool isOffsetToSkipSetFFIDGPWARequired(const HardwareInfo &hwInfo) {
    HwHelper &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    return hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo);
}

bool isForceDefaultRCSEngineWARequired(const HardwareInfo &hwInfo) {
    return ((hwInfo.platform.eProductFamily == IGFX_TIGERLAKE_LP));
}

bool isForceEmuInt32DivRemSPWARequired(const HardwareInfo &hwInfo) {
    HwHelper &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    return ((hwInfo.platform.eProductFamily == IGFX_TIGERLAKE_LP) & hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo));
}

bool is3DPipelineSelectWARequired(const HardwareInfo &hwInfo) {
    return hwInfo.platform.eProductFamily == IGFX_TIGERLAKE_LP;
}

} // namespace Gen12LPHelpers
} // namespace NEO
