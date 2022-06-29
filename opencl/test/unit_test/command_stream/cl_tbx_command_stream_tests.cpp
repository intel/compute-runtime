/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/engine_control.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/mocks/mock_tbx_csr.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;

using ClTbxCommandStreamTests = Test<ClDeviceFixture>;
HWTEST_F(ClTbxCommandStreamTests, givenTbxCsrWhenDispatchBlitEnqueueThenProcessCorrectly) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableBlitterOperationsSupport.set(1);
    DebugManager.flags.EnableBlitterForEnqueueOperations.set(1);

    MockContext context(pClDevice);

    MockTbxCsr<FamilyType> tbxCsr0{*pDevice->executionEnvironment, pDevice->getDeviceBitfield()};
    tbxCsr0.initializeTagAllocation();
    MockTbxCsr<FamilyType> tbxCsr1{*pDevice->executionEnvironment, pDevice->getDeviceBitfield()};
    tbxCsr1.initializeTagAllocation();

    MockOsContext osContext0(0, EngineDescriptorHelper::getDefaultDescriptor(pDevice->getDeviceBitfield()));
    tbxCsr0.setupContext(osContext0);
    EngineControl engineControl0{&tbxCsr0, &osContext0};

    MockOsContext osContext1(1, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular}, pDevice->getDeviceBitfield()));
    tbxCsr1.setupContext(osContext0);
    EngineControl engineControl1{&tbxCsr1, &osContext1};

    MockCommandQueueHw<FamilyType> cmdQ(&context, pClDevice, nullptr);
    cmdQ.gpgpuEngine = &engineControl0;
    cmdQ.clearBcsEngines();
    cmdQ.bcsEngines[0] = &engineControl1;

    cl_int error = CL_SUCCESS;
    std::unique_ptr<Buffer> buffer(Buffer::create(&context, 0, 1, nullptr, error));

    uint32_t hostPtr = 0;
    error = cmdQ.enqueueWriteBuffer(buffer.get(), CL_TRUE, 0, 1, &hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, error);
}
