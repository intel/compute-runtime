/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/device_command_stream_fixture.h"

#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

TEST(OSContextLinux, givenReinitializeContextWhenContextIsInitThenContextIsStillIinitializedAfter) {
    MockExecutionEnvironment executionEnvironment;
    std::unique_ptr<DrmMockCustom> mock(new DrmMockCustom(*executionEnvironment.rootDeviceEnvironments[0]));
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock.get(), 0u);
    OsContextLinux osContext(*mock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    EXPECT_NO_THROW(osContext.reInitializeContext());
    EXPECT_NO_THROW(osContext.ensureContextInitialized());
}

TEST(OSContextLinux, givenInitializeContextWhenContextCreateIoctlFailsThenContextNotInitialized) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock *pDrm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    pDrm->storedRetVal = -1;
    EXPECT_EQ(-1, pDrm->createDrmContext(1, false, false));

    OsContextLinux osContext(*pDrm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    EXPECT_EQ(false, osContext.ensureContextInitialized());
    delete pDrm;
}