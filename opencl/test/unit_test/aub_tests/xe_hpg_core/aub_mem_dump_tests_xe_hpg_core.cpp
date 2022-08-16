/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "opencl/test/unit_test/aub_tests/command_stream/aub_mem_dump_tests.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

namespace NEO {
extern bool overrideCommandStreamReceiverCreation;
using XeHpgCoreAubMemDumpTests = Test<ClDeviceFixture>;

XE_HPG_CORETEST_F(XeHpgCoreAubMemDumpTests, GivenCcsThenExpectationsAreMet) {
    setupAUB<FamilyType>(pDevice, aub_stream::ENGINE_CCS);
}

XE_HPG_CORETEST_F(XeHpgCoreAubMemDumpTests, whenAubCsrIsCreatedThenCreateHardwareContext) {
    DebugManagerStateRestore restore;
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useHwCsr = true;
    DebugManager.flags.SetCommandStreamReceiver.set(CommandStreamReceiverType::CSR_AUB);

    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    auto &baseCsr = device->getGpgpuCommandStreamReceiver();
    auto &aubCsr = static_cast<AUBCommandStreamReceiverHw<FamilyType> &>(baseCsr);

    EXPECT_NE(nullptr, aubCsr.hardwareContextController.get());
    EXPECT_NE(0u, aubCsr.hardwareContextController->hardwareContexts.size());
}
} // namespace NEO
