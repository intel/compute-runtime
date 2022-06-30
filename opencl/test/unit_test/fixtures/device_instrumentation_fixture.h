/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <memory>

namespace NEO {

class ClDevice;
class Device;
struct HardwareInfo;

struct DeviceInstrumentationFixture {
    void SetUp(bool instrumentation); // NOLINT(readability-identifier-naming)

    std::unique_ptr<ClDevice> device = nullptr;
    HardwareInfo *hwInfo = nullptr;
};
} // namespace NEO
