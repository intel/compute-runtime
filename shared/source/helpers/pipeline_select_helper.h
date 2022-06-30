/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace NEO {
const uint32_t pipelineSelectEnablePipelineSelectMaskBits = 0x3;
const uint32_t pipelineSelectMediaSamplerDopClockGateMaskBits = 0x10;
const uint32_t pipelineSelectMediaSamplerPowerClockGateMaskBits = 0x40;
const uint32_t pipelineSelectSystolicModeEnableMaskBits = 0x80;
} // namespace NEO
