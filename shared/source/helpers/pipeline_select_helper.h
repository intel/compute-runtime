/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace NEO {
constexpr uint32_t pipelineSelectEnablePipelineSelectMaskBits = 0x3;
constexpr uint32_t pipelineSelectMediaSamplerDopClockGateMaskBits = 0x10;
constexpr uint32_t pipelineSelectMediaSamplerPowerClockGateMaskBits = 0x40;
constexpr uint32_t pipelineSelectSystolicModeEnableMaskBits = 0x80;
} // namespace NEO
