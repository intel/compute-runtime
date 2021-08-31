/*
 * Copyright (C) 2019-2021 Intel Corporation
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
bool imagePitchAlignmentWaRequired(PRODUCT_FAMILY productFamily);
void adjustCoherencyFlag(PRODUCT_FAMILY productFamily, bool &coherencyFlag);
bool isLocalMemoryEnabled(const HardwareInfo &hwInfo);
void initAdditionalGlobalMMIO(const CommandStreamReceiver &commandStreamReceiver, AubMemDump::AubStream &stream);
uint64_t getPPGTTAdditionalBits(GraphicsAllocation *graphicsAllocation);
void adjustAubGTTData(const CommandStreamReceiver &commandStreamReceiver, AubGTTData &data);
bool isOffsetToSkipSetFFIDGPWARequired(const HardwareInfo &hwInfo);
bool isForceEmuInt32DivRemSPWARequired(const HardwareInfo &hwInfo);
bool is3DPipelineSelectWARequired(const HardwareInfo &hwInfo);
inline void adjustPlatformForProductFamily(PLATFORM &platform, GFXCORE_FAMILY newCoreFamily) {
    platform.eRenderCoreFamily = IGFX_GEN12LP_CORE;
    platform.eDisplayCoreFamily = IGFX_GEN12LP_CORE;
}

} // namespace Gen12LPHelpers
} // namespace NEO
