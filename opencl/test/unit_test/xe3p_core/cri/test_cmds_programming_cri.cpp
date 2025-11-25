/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/state_base_address.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "hw_cmds_xe3p_core.h"
#include "per_product_test_definitions.h"

using namespace NEO;
using CmdsProgrammingTestsCri = UltCommandStreamReceiverTest;

CRITEST_F(CmdsProgrammingTestsCri, givenEnable64BitAddressingWhenSbaIsProgrammedThenHeaplessModeIsEnabled) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    DebugManagerStateRestore restore;

    struct TestParam {
        int32_t enable64BitAddressing;
        bool expectedResult;
    };

    TestParam testParams[3] = {{-1, true}, {0, false}, {1, true}};

    for (auto &[enable64BitAddressing, expectedResult] : testParams) {
        debugManager.flags.Enable64BitAddressing.set(enable64BitAddressing);
        int32_t executionStamp = 0;
        auto mockCSR = new MockCsr<FamilyType>(executionStamp, *pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
        pDevice->resetCommandStreamReceiver(mockCSR);
        auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
        flushTask(commandStreamReceiver);

        HardwareParse hwParserCsr;
        hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
        hwParserCsr.findHardwareCommands<FamilyType>();
        ASSERT_NE(nullptr, hwParserCsr.cmdStateBaseAddress);

        auto stateBaseAddress = static_cast<STATE_BASE_ADDRESS *>(hwParserCsr.cmdStateBaseAddress);
        auto heaplessModeEnabled = stateBaseAddress->getBaseAddressDisable();

        EXPECT_EQ(expectedResult, heaplessModeEnabled);
    }
}
