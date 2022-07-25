/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
uint32_t HwInfoConfigHw<gfxProduct>::getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const {
    switch (stepping) {
    case REVISION_A0:
        return 0x0;
    case REVISION_B:
        return 0x4;
    }
    return CommonConstants::invalidStepping;
}

template <>
AOT::PRODUCT_CONFIG HwInfoConfigHw<gfxProduct>::getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const {
    return AOT::ADL_P;
}

template <>
uint32_t HwInfoConfigHw<gfxProduct>::getSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
    switch (hwInfo.platform.usRevId) {
    case 0x0:
        return REVISION_A0;
    case 0x4:
        return REVISION_B;
    }
    return CommonConstants::invalidStepping;
}

template <>
void HwInfoConfigHw<gfxProduct>::setAdditionalPipelineSelectFields(void *pipelineSelectCmd,
                                                                   const PipelineSelectArgs &pipelineSelectArgs,
                                                                   const HardwareInfo &hwInfo) {
    using PIPELINE_SELECT = typename Gen12LpFamily::PIPELINE_SELECT;
    auto pipelineSelectTglplpCmd = reinterpret_cast<PIPELINE_SELECT *>(pipelineSelectCmd);

    auto mask = pipelineSelectTglplpCmd->getMaskBits();

    mask |= pipelineSelectSystolicModeEnableMaskBits;
    pipelineSelectTglplpCmd->setMaskBits(mask);
    pipelineSelectTglplpCmd->setSpecialModeEnable(pipelineSelectArgs.specialPipelineSelectMode);
}
