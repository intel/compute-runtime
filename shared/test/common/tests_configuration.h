/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {
struct HardwareInfo;
enum class TestMode { notSpecified,
                      unitTests,
                      aubTests,
                      aubTestsWithTbx,
                      tbxTests,
                      aubTestsWithoutOutputFiles
};

inline bool isAubTestMode(TestMode testMode) {
    return testMode == TestMode::aubTests ||
           testMode == TestMode::aubTestsWithTbx ||
           testMode == TestMode::aubTestsWithoutOutputFiles;
}

extern TestMode testMode;

void adjustHwInfoForTests(HardwareInfo &hwInfoForTests, uint32_t euPerSubSlice, uint32_t sliceCount, uint32_t subSlicePerSliceCount, int dieRecovery);
void adjustCsrType(TestMode testMode);
} // namespace NEO
