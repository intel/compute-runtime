/*
 * Copyright (C) 2018-2025 Intel Corporation
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
    void setUp(bool instrumentation);

    std::unique_ptr<ClDevice> device;
    HardwareInfo *hwInfo = nullptr;
};
} // namespace NEO
