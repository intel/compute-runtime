/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/gen12lp/hw_cmds_base.h"

namespace AubMemDump {
struct AubStream;
}
struct AubGTTData;

namespace NEO {
class CommandStreamReceiver;
class GraphicsAllocation;
struct PipelineSelectArgs;
class Image;

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
bool isPageTableManagerSupported(const HardwareInfo &hwInfo);
bool obtainRenderBufferCompressionPreference(const HardwareInfo &hwInfo, const size_t size);
void setAdditionalSurfaceStateParamsForImageCompression(Image &image, typename TGLLPFamily::RENDER_SURFACE_STATE *surfaceState);
} // namespace Gen12LPHelpers
} // namespace NEO
