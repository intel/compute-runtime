/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <memory>

namespace NEO {

class ClDevice;
class Device;
struct HardwareInfo;

struct DeviceInstrumentationFixture {
    void SetUp(bool instrumentation);

    std::unique_ptr<ClDevice> device = nullptr;
    HardwareInfo *hwInfo = nullptr;
};
} // namespace NEO
