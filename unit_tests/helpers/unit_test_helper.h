/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace OCLRT {

class Kernel;
struct HardwareInfo;

template <typename GfxFamily>
struct UnitTestHelper {
    static bool isL3ConfigProgrammable();

    static bool evaluateDshUsage(size_t sizeBeforeEnqueue, size_t sizeAfterEnqueue, Kernel *kernel);

    static bool isPageTableManagerSupported(const HardwareInfo &hwInfo);

    static bool isTimestampPacketWriteSupported();
};
} // namespace OCLRT
