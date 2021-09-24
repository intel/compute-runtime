/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/helpers_gen12lp.h"

#include "shared/source/command_stream/command_stream_receiver.h"

#include "opencl/source/helpers/hardware_commands_helper.h"

namespace NEO {
namespace Gen12LPHelpers {

void adjustCoherencyFlag(PRODUCT_FAMILY productFamily, bool &coherencyFlag) {}

void initAdditionalGlobalMMIO(const CommandStreamReceiver &commandStreamReceiver, AubMemDump::AubStream &stream) {}

uint64_t getPPGTTAdditionalBits(GraphicsAllocation *graphicsAllocation) {
    return 0;
}

void adjustAubGTTData(const CommandStreamReceiver &commandStreamReceiver, AubGTTData &data) {}

bool isOffsetToSkipSetFFIDGPWARequired(const HardwareInfo &hwInfo) {
    HwHelper &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    return hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo);
}

} // namespace Gen12LPHelpers
} // namespace NEO
