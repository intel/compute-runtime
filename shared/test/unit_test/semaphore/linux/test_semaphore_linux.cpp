/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/os_interface/external_semaphore.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/os_interface/linux/drm_memory_manager_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

namespace NEO {

using DrmExternalSemaphoreTest = Test<DrmMemoryManagerFixture>;

TEST_F(DrmExternalSemaphoreTest, givenNullOsInterfaceWhenCreateExternalSemaphoreIsCalledThenNullptrIsReturned) {
    auto externalSemaphore = ExternalSemaphore::create(nullptr, ExternalSemaphore::Type::OpaqueFd, nullptr, 0u, nullptr);
    EXPECT_EQ(externalSemaphore, nullptr);
}

} // namespace NEO