/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/windows/wddm_memory_operations_handler.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/mem_obj/mem_obj.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

using namespace NEO;

TEST(WddmResidentBufferTests, whenBuffersIsCreatedWithMakeResidentFlagSetThenItIsMadeResidentUponCreation) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    ultHwConfig.forceOsAgnosticMemoryManager = false;
    DebugManagerStateRestore restorer;
    debugManager.flags.MakeAllBuffersResident.set(true);

    initPlatform();
    auto device = platform()->getClDevice(0u);

    MockContext context(device, false);
    auto retValue = CL_SUCCESS;
    auto clBuffer = clCreateBuffer(&context, 0u, 4096u, nullptr, &retValue);
    ASSERT_EQ(retValue, CL_SUCCESS);

    auto memoryOperationsHandler = device->getRootDeviceEnvironment().memoryOperationsInterface.get();
    auto neoBuffer = castToObject<MemObj>(clBuffer);
    auto bufferAllocation = neoBuffer->getGraphicsAllocation(device->getRootDeviceIndex());
    auto status = memoryOperationsHandler->isResident(nullptr, *bufferAllocation);

    EXPECT_EQ(status, MemoryOperationsStatus::success);

    clReleaseMemObject(clBuffer);
}
