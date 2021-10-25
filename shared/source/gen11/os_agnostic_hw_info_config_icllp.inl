/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
bool HwInfoConfigHw<gfxProduct>::isAdditionalMediaSamplerProgrammingRequired() const {
    return true;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isInitialFlagsProgrammingRequired() const {
    return true;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isReturnedCmdSizeForMediaSamplerAdjustmentRequired() const {
    return true;
}
