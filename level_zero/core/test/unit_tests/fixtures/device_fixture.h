/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/unit_test/helpers/default_hw_info.h"

#include "opencl/test/unit_test/mocks/mock_device.h"

#include "level_zero/core/test/unit_tests/mocks/mock_device.h"

namespace L0 {
namespace ult {

struct DeviceFixture {
    void SetUp() {
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
        device = std::make_unique<Mock<L0::DeviceImp>>(neoDevice, neoDevice->getExecutionEnvironment());
    }

    void TearDown() {
    }

    NEO::MockDevice *neoDevice = nullptr;
    std::unique_ptr<Mock<L0::DeviceImp>> device = nullptr;
};

} // namespace ult
} // namespace L0
