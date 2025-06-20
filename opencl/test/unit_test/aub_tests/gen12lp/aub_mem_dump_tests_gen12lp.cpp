/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/aub_command_stream_receiver_hw.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/test/unit_test/aub_tests/command_stream/aub_mem_dump_tests.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

namespace NEO {

using Gen12LPAubMemDumpTests = Test<ClDeviceFixture>;

GEN12LPTEST_F(Gen12LPAubMemDumpTests, whenAubCsrIsCreatedThenCreateHardwareContext) {
    DebugManagerStateRestore restore;
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useHwCsr = true;

    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    auto &baseCsr = device->getGpgpuCommandStreamReceiver();
    auto &aubCsr = static_cast<AUBCommandStreamReceiverHw<FamilyType> &>(baseCsr);

    EXPECT_NE(nullptr, aubCsr.hardwareContextController.get());
    EXPECT_NE(0u, aubCsr.hardwareContextController->hardwareContexts.size());
}

} // namespace NEO
