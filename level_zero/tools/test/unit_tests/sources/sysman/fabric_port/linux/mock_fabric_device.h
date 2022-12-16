/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include "gmock/gmock.h"
#include "sysman/fabric_port/fabric_port.h"

namespace L0 {
namespace ult {

const uint32_t mockNumPorts = 2;
struct MockFabricDevice : public FabricDevice {
    uint32_t getNumPorts() override {
        return mockNumPorts;
    }

    ADDMETHOD_NOBASE(getOsFabricDevice, OsFabricDevice *, nullptr, ());
    MockFabricDevice() = default;
};

} // namespace ult
} // namespace L0
