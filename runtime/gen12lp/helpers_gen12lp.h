/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "hw_info.h"

namespace AubMemDump {
struct AubStream;
}
struct AubGTTData;

namespace NEO {
class CommandStreamReceiver;
class GraphicsAllocation;
struct PipelineSelectArgs;

namespace Gen12LPHelpers {
bool hdcFlushForPipeControlBeforeStateBaseAddressRequired(PRODUCT_FAMILY productFamily);
bool pipeControlWaRequired(PRODUCT_FAMILY productFamily);
bool imagePitchAlignmentWaRequired(PRODUCT_FAMILY productFamily);
void adjustCoherencyFlag(PRODUCT_FAMILY productFamily, bool &coherencyFlag);
bool isLocalMemoryEnabled(const HardwareInfo &hwInfo);
void initAdditionalGlobalMMIO(const CommandStreamReceiver &commandStreamReceiver, AubMemDump::AubStream &stream);
uint64_t getPPGTTAdditionalBits(GraphicsAllocation *graphicsAllocation);
void adjustAubGTTData(const CommandStreamReceiver &commandStreamReceiver, AubGTTData &data);
void setAdditionalPipelineSelectFields(void *pipelineSelectCmd,
                                       const PipelineSelectArgs &pipelineSelectArgs,
                                       const HardwareInfo &hwInfo);
} // namespace Gen12LPHelpers
} // namespace NEO
