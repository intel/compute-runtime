/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/fixtures/aub_fixtures/multicontext_aub_fixture.h"

#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

#include <memory>
#include <vector>

namespace L0 {
struct Device;
struct DriverHandleImp;
;

namespace ult {
template <typename Type>
struct Mock;
template <typename Type>
struct WhiteBox;
} // namespace ult
} // namespace L0

struct MulticontextL0AubFixture : public NEO::MulticontextAubFixture {
    void setUp(uint32_t numberOfTiles, EnabledCommandStreamers enabledCommandStreamers, bool implicitScaling);

    CommandStreamReceiver *getGpgpuCsr(uint32_t tile, uint32_t engine) override;
    void createDevices(const NEO::HardwareInfo &hwInfo, uint32_t numTiles) override;

    std::unique_ptr<L0::ult::Mock<L0::DriverHandleImp>> driverHandle;
    NEO::ExecutionEnvironment *executionEnvironment = nullptr;
    std::vector<L0::Device *> subDevices;
    L0::Device *rootDevice = nullptr;
};
