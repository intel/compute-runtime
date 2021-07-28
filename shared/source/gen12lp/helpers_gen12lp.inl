/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/pipeline_select_helper.h"

namespace NEO {
namespace Gen12LPHelpers {
void setAdditionalPipelineSelectFields(void *pipelineSelectCmd,
                                       const PipelineSelectArgs &pipelineSelectArgs,
                                       const HardwareInfo &hwInfo) {
    using PIPELINE_SELECT = typename TGLLPFamily::PIPELINE_SELECT;
    auto pipelineSelectTglplpCmd = reinterpret_cast<PIPELINE_SELECT *>(pipelineSelectCmd);

    auto mask = pipelineSelectTglplpCmd->getMaskBits();

    if (hwInfo.platform.eProductFamily == IGFX_ALDERLAKE_P) {
        mask |= pipelineSelectSystolicModeEnableMaskBits;
        pipelineSelectTglplpCmd->setMaskBits(mask);
        pipelineSelectTglplpCmd->setSpecialModeEnable(pipelineSelectArgs.specialPipelineSelectMode);
    }
}
} // namespace Gen12LPHelpers
} // namespace NEO
