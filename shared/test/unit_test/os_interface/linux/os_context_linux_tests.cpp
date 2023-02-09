/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
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
