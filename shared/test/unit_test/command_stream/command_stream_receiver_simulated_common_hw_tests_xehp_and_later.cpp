/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_helper.h"
#include "shared/source/command_stream/command_stream_receiver_simulated_common_hw.h"
#include "shared/source/helpers/hardware_context_controller.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/mocks/mock_csr_simulated_common_hw.h"

using XeHPAndLaterMockSimulatedCsrHwTests = Test<DeviceFixture>;

class XeHPAndLaterTileRangeRegisterTest : public DeviceFixture, public ::testing::Test {
  public:
    template <typename FamilyType>
    void setUpImpl() {
        hardwareInfo = *defaultHwInfo;
        releaseHelper = ReleaseHelper::create(hardwareInfo.ipVersion);
        hardwareInfoSetup[hardwareInfo.platform.eProductFamily](&hardwareInfo, true, 0, releaseHelper.get());
        hardwareInfo.gtSystemInfo.MultiTileArchInfo.IsValid = true;
        DeviceFixture::setUpImpl(&hardwareInfo);
    }

    void SetUp() override {
    }

    void TearDown() override {
        DeviceFixture::tearDown();
    }

    std::unique_ptr<ReleaseHelper> releaseHelper;
};
