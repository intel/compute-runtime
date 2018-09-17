/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <memory>

namespace OCLRT {

class Device;
struct HardwareInfo;

struct DeviceInstrumentationFixture {
    void SetUp(bool instrumentation);

    std::unique_ptr<Device> device = nullptr;
    HardwareInfo *hwInfo = nullptr;
};
} // namespace OCLRT