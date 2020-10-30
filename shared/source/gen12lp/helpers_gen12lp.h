/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/gen12lp/hw_cmds_base.h"

namespace AubMemDump {
struct AubStream;
}
struct AubGTTData;

namespace NEO {
class CommandStreamReceiver;
class GraphicsAllocation;
struct HardwareInfo;
struct PipelineSelectArgs;
class Image;
enum class LocalMemoryAccessMode;

namespace Gen12LPHelpers {
bool pipeControlWaRequired(PRODUCT_FAMILY productFamily);
uint32_t getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo);
uint32_t getSteppingFromHwRevId(uint32_t hwRevId, const HardwareInfo &hwInfo);
bool imagePitchAlignmentWaRequired(PRODUCT_FAMILY productFamily);
void adjustCoherencyFlag(PRODUCT_FAMILY productFamily, bool &coherencyFlag);
bool isLocalMemoryEnabled(const HardwareInfo &hwInfo);
void initAdditionalGlobalMMIO(const CommandStreamReceiver &commandStreamReceiver, AubMemDump::AubStream &stream);
uint64_t getPPGTTAdditionalBits(GraphicsAllocation *graphicsAllocation);
void adjustAubGTTData(const CommandStreamReceiver &commandStreamReceiver, AubGTTData &data);
void setAdditionalPipelineSelectFields(void *pipelineSelectCmd,
                                       const PipelineSelectArgs &pipelineSelectArgs,
                                       const HardwareInfo &hwInfo);
bool isOffsetToSkipSetFFIDGPWARequired(const HardwareInfo &hwInfo);
bool isForceEmuInt32DivRemSPWARequired(const HardwareInfo &hwInfo);
bool is3DPipelineSelectWARequired(const HardwareInfo &hwInfo);
bool forceBlitterUseForGlobalBuffers(const HardwareInfo &hwInfo, GraphicsAllocation *allocation);
bool obtainBlitterPreference(const HardwareInfo &hwInfo);
LocalMemoryAccessMode getDefaultLocalMemoryAccessMode(const HardwareInfo &hwInfo);

} // namespace Gen12LPHelpers
} // namespace NEO
