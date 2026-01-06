/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/external_semaphore.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

namespace NEO {

using WddmExternalSemaphoreTest = WddmFixture;

TEST_F(WddmExternalSemaphoreTest, givenOpaqueFdSemaphoreWhenCreateExternalSemaphoreIsCalledThenNullptrIsReturned) {
    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::OpaqueFd, nullptr, 0u, nullptr);
    EXPECT_EQ(externalSemaphore, nullptr);
}

TEST_F(WddmExternalSemaphoreTest, givenNullOsInterfaceWhenCreateExternalSemaphoreIsCalledThenNullptrIsReturned) {
    HANDLE extSemaphoreHandle = 0;

    auto externalSemaphore = ExternalSemaphore::create(nullptr, ExternalSemaphore::Type::D3d12Fence, extSemaphoreHandle, 0u, nullptr);
    EXPECT_EQ(externalSemaphore, nullptr);
}

TEST_F(WddmExternalSemaphoreTest, givenValidD3d12FenceSemaphoreWhenCreateExternalSemaphoreIsCalledThenSemaphoreIsSuccessfullyReturned) {
    HANDLE extSemaphoreHandle = 0;

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::D3d12Fence, extSemaphoreHandle, 0u, nullptr);
    EXPECT_NE(externalSemaphore, nullptr);
}

TEST_F(WddmExternalSemaphoreTest, givenValidTimelineSemaphoreWin32WhenCreateExternalSemaphoreIsCalledThenSemaphoreIsSuccessfullyReturned) {
    HANDLE extSemaphoreHandle = 0;
    const char *extSemName = "timeline_semaphore_name";

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::TimelineSemaphoreWin32, extSemaphoreHandle, 0u, extSemName);
    EXPECT_NE(externalSemaphore, nullptr);
}

class MockFailGdi : public MockGdi {
  public:
    MockFailGdi() : MockGdi() {
        openSyncObjectNtHandleFromName = mockD3DKMTOpenSyncObjectNtHandleFromName;
        openSyncObjectFromNtHandle2 = mockD3DKMTOpenSyncObjectFromNtHandle2;
    }

    static NTSTATUS __stdcall mockD3DKMTOpenSyncObjectNtHandleFromName(IN OUT D3DKMT_OPENSYNCOBJECTNTHANDLEFROMNAME *) {
        return STATUS_UNSUCCESSFUL;
    }

    static NTSTATUS __stdcall mockD3DKMTOpenSyncObjectFromNtHandle2(IN OUT D3DKMT_OPENSYNCOBJECTFROMNTHANDLE2 *) {
        return STATUS_UNSUCCESSFUL;
    }
};

TEST_F(WddmExternalSemaphoreTest, givenTimelineSemaphoreWin32FailsToOpenSyncObjectFromNameThenNullptrIsReturned) {
    auto mockGdi = new MockFailGdi();
    static_cast<OsEnvironmentWin *>(executionEnvironment->osEnvironment.get())->gdi.reset(mockGdi);
    HANDLE extSemaphoreHandle = 0;
    const char *extSemName = "timeline_semaphore_name";

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::TimelineSemaphoreWin32, extSemaphoreHandle, 0u, extSemName);
    EXPECT_EQ(externalSemaphore, nullptr);
}

TEST_F(WddmExternalSemaphoreTest, givenTimelineSemaphoreWin32FailsToOpenSyncObjectFromNtHandleThenNullptrIsReturned) {
    auto mockGdi = new MockFailGdi();
    static_cast<OsEnvironmentWin *>(executionEnvironment->osEnvironment.get())->gdi.reset(mockGdi);
    HANDLE extSemaphoreHandle = 0;

    auto externalSemaphore = ExternalSemaphore::create(osInterface, ExternalSemaphore::Type::TimelineSemaphoreWin32, extSemaphoreHandle, 0u, nullptr);
    EXPECT_EQ(externalSemaphore, nullptr);
}

} // namespace NEO
