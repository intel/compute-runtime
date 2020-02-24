/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/gen12lp/helpers_gen12lp.h"

#include "shared/source/command_stream/command_stream_receiver.h"

#include "opencl/source/command_stream/command_stream_receiver_simulated_common_hw.h"

namespace NEO {
namespace Gen12LPHelpers {

bool pipeControlWaRequired(PRODUCT_FAMILY productFamily) {
    return (productFamily == PRODUCT_FAMILY::IGFX_TIGERLAKE_LP);
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
    return (hwInfo.platform.usRevId < REVISION_B);
}

bool isForceDefaultRCSEngineWARequired(const HardwareInfo &hwInfo) {
    return ((hwInfo.platform.eProductFamily == IGFX_TIGERLAKE_LP) & (hwInfo.platform.usRevId < REVISION_B));
}

bool isForceEmuInt32DivRemSPWARequired(const HardwareInfo &hwInfo) {
    return ((hwInfo.platform.eProductFamily == IGFX_TIGERLAKE_LP) & (hwInfo.platform.usRevId < REVISION_B));
}

} // namespace Gen12LPHelpers
} // namespace NEO
