/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <memory>

namespace NEO {

class Device;
struct HardwareInfo;

struct DeviceInstrumentationFixture {
    void SetUp(bool instrumentation);

    std::unique_ptr<Device> device = nullptr;
    HardwareInfo *hwInfo = nullptr;
};
} // namespace NEO