/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include "sysman/fabric_port/fabric_port.h"

namespace L0 {
namespace ult {

template <>
struct Mock<FabricDevice> : public FabricDevice {
    MOCK_METHOD(uint32_t, getNumPorts, (), (override));
    MOCK_METHOD(OsFabricDevice *, getOsFabricDevice, (), (override));

    Mock() = default;
    ~Mock() override = default;
};

} // namespace ult
} // namespace L0
