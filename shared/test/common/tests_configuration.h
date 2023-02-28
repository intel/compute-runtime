/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {
struct HardwareInfo;
enum class TestMode { NotSpecified,
                      UnitTests,
                      AubTests,
                      AubTestsWithTbx,
                      TbxTests };

extern TestMode testMode;

void adjustHwInfoForTests(HardwareInfo &hwInfoForTests, uint32_t euPerSubSlice, uint32_t sliceCount, uint32_t subSlicePerSliceCount, int dieRecovery);
} // namespace NEO
