/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/hw_device_id.h"
#include "shared/source/os_interface/windows/os_environment_win.h"
#include "shared/source/os_interface/windows/pdh_interface.h"
#include "shared/source/os_interface/windows/wddm/um_km_data_translator.h"
#include "shared/source/os_interface/windows/wddm/wddm_interface.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "gtest/gtest.h"

using namespace NEO;

namespace NEO {
extern bool returnEmptyFilesVector;
extern int setErrno;
extern std::map<std::string, std::vector<std::string>> directoryFilesMap;
} // namespace NEO

TEST(DrmOrWddmTest, GivenAccessDeniedWhenDiscoveringDevicesThenDevicePermissionErrorIsNotSet) {
    SysCalls::openFuncCalled = 0;
    VariableBackup<decltype(SysCalls::openFuncCalled)> openCounter(&SysCalls::openFuncCalled);
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        errno = EACCES;
        return -1;
    });
    VariableBackup<bool> emptyDir(&NEO::returnEmptyFilesVector, true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    EXPECT_FALSE(executionEnvironment->isDevicePermissionError());
    auto devices = OSInterface::discoverDevices(*executionEnvironment);
    EXPECT_FALSE(executionEnvironment->isDevicePermissionError());
    EXPECT_FALSE(devices.empty());
    EXPECT_NE(0u, SysCalls::openFuncCalled);
}

TEST(DrmOrWddmTest, GivenInufficientPermissionsWhenDiscoveringDevicesThenDevicePermissionErrorIsNotSet) {
    SysCalls::openFuncCalled = 0;
    VariableBackup<decltype(SysCalls::openFuncCalled)> openCounter(&SysCalls::openFuncCalled);
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        errno = EPERM;
        return -1;
    });
    VariableBackup<bool> emptyDir(&NEO::returnEmptyFilesVector, true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    EXPECT_FALSE(executionEnvironment->isDevicePermissionError());
    auto devices = OSInterface::discoverDevices(*executionEnvironment);
    EXPECT_FALSE(executionEnvironment->isDevicePermissionError());
    EXPECT_FALSE(devices.empty());
    EXPECT_NE(0u, SysCalls::openFuncCalled);
}

TEST(DrmOrWddmTest, GivenOtherErrorWhenDiscoveringDevicesThenDevicePermissionErrorIsNotSet) {
    SysCalls::openFuncCalled = 0;
    VariableBackup<decltype(SysCalls::openFuncCalled)> openCounter(&SysCalls::openFuncCalled);
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        errno = ENOENT;
        return -1;
    });
    VariableBackup<bool> emptyDir(&NEO::returnEmptyFilesVector, true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    EXPECT_FALSE(executionEnvironment->isDevicePermissionError());
    auto devices = OSInterface::discoverDevices(*executionEnvironment);
    EXPECT_FALSE(executionEnvironment->isDevicePermissionError());
    EXPECT_FALSE(devices.empty());
    EXPECT_NE(0u, SysCalls::openFuncCalled);
}

TEST(DrmOrWddmTest, GivenAccessDeniedWithNonEmptyFilesListWhenDiscoveringDevicesThenDevicePermissionErrorIsNotSet) {
    SysCalls::openFuncCalled = 0;
    VariableBackup<decltype(SysCalls::openFuncCalled)> openCounter(&SysCalls::openFuncCalled);
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        errno = EACCES;
        return -1;
    });

    NEO::directoryFilesMap.insert({"/dev/dri/by-path/pci-0000:00:01.0-render", {"unknown"}});
    NEO::directoryFilesMap.insert({"/dev/dri/by-path/pci-0000:00:02.0-render", {"unknown"}});
    NEO::directoryFilesMap.insert({"/dev/dri/by-path/pci-0000:00:03.0-render", {"unknown"}});

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    EXPECT_FALSE(executionEnvironment->isDevicePermissionError());
    auto devices = OSInterface::discoverDevices(*executionEnvironment);
    EXPECT_FALSE(executionEnvironment->isDevicePermissionError());
    EXPECT_FALSE(devices.empty());
    EXPECT_NE(0u, SysCalls::openFuncCalled);

    NEO::directoryFilesMap.clear();
}

TEST(DrmOrWddmTest, GivenInufficientPermissionsWithNonEmptyFilesListWhenDiscoveringDevicesThenDevicePermissionErrorIsNotSet) {
    SysCalls::openFuncCalled = 0;
    VariableBackup<decltype(SysCalls::openFuncCalled)> openCounter(&SysCalls::openFuncCalled);
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        errno = EPERM;
        return -1;
    });

    NEO::directoryFilesMap.insert({"/dev/dri/by-path/pci-0000:00:01.0-render", {"unknown"}});
    NEO::directoryFilesMap.insert({"/dev/dri/by-path/pci-0000:00:02.0-render", {"unknown"}});
    NEO::directoryFilesMap.insert({"/dev/dri/by-path/pci-0000:00:03.0-render", {"unknown"}});

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    EXPECT_FALSE(executionEnvironment->isDevicePermissionError());
    auto devices = OSInterface::discoverDevices(*executionEnvironment);
    EXPECT_FALSE(executionEnvironment->isDevicePermissionError());
    EXPECT_FALSE(devices.empty());
    EXPECT_NE(0u, SysCalls::openFuncCalled);

    NEO::directoryFilesMap.clear();
}

TEST(DrmOrWddmTest, GivenAccessDeniedForDirectoryWhenDiscoveringDevicesThenDevicePermissionErrorIsNotSet) {
    SysCalls::openFuncCalled = 0;
    VariableBackup<decltype(SysCalls::openFuncCalled)> openCounter(&SysCalls::openFuncCalled);
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        errno = ENOENT;
        return -1;
    });
    VariableBackup<bool> emptyDir(&NEO::returnEmptyFilesVector, true);
    VariableBackup<int> errnoNumber(&NEO::setErrno, EACCES);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    EXPECT_FALSE(executionEnvironment->isDevicePermissionError());
    auto devices = OSInterface::discoverDevices(*executionEnvironment);
    EXPECT_FALSE(executionEnvironment->isDevicePermissionError());
    EXPECT_FALSE(devices.empty());
    EXPECT_NE(0u, SysCalls::openFuncCalled);
}

TEST(DrmOrWddmTest, GivenInufficientPermissionsForDirectoryWhenDiscoveringDevicesThenDevicePermissionErrorIsNotSet) {
    SysCalls::openFuncCalled = 0;
    VariableBackup<decltype(SysCalls::openFuncCalled)> openCounter(&SysCalls::openFuncCalled);
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        errno = ENOENT;
        return -1;
    });
    VariableBackup<bool> emptyDir(&NEO::returnEmptyFilesVector, true);
    VariableBackup<int> errnoNumber(&NEO::setErrno, EPERM);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    EXPECT_FALSE(executionEnvironment->isDevicePermissionError());
    auto devices = OSInterface::discoverDevices(*executionEnvironment);
    EXPECT_FALSE(executionEnvironment->isDevicePermissionError());
    EXPECT_FALSE(devices.empty());
    EXPECT_NE(0u, SysCalls::openFuncCalled);
}

TEST(DrmOrWddmTest, givenWddmWhenSetGmmInputArgsThenFileDescriptorIsSetToAdapterBdfData) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto wddm = std::make_unique<WddmMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    uint32_t expectedBdf = 1234u;
    wddm->adapterBDF.data = expectedBdf;

    GMM_INIT_IN_ARGS gmmInArgs = {};
    wddm->setGmmInputArgs(&gmmInArgs);

    EXPECT_EQ(expectedBdf, gmmInArgs.FileDescriptor);
}

TEST(DrmOrWddmTest, givenWslWhenCreateNativeFenceThenFail) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto wddm = std::make_unique<WddmMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    WddmSyncFence syncFence;
    syncFence.setFenceValue(0u);
    EXPECT_EQ(syncFence.getFence()->currentFenceValue, 0u);

    wddm->wddmInterface = std::make_unique<WddmInterface23>(*wddm);

    EXPECT_FALSE(wddm->getWddmInterface()->createNativeFence(*syncFence.getFence(), false));
    EXPECT_EQ(syncFence.getCpuAddress(), nullptr);
    EXPECT_EQ(syncFence.getGpuAddress(), 0u);
    EXPECT_EQ(syncFence.getFence()->currentFenceValue, 0u);
}

TEST(DrmOrWddmTest, givenWslWhenCreatingPdhInterfaceThenNullptrIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1u);
    auto pdhInterface = PdhInterface::create(*executionEnvironment);

    EXPECT_EQ(nullptr, pdhInterface);
}

namespace {
struct ShareObjectsCapture {
    static NTSTATUS APIENTRY shareObjects(UINT cObjects, const D3DKMT_HANDLE *hObjects,
                                          POBJECT_ATTRIBUTES pObjectAttributes, DWORD dwDesiredAccess,
                                          HANDLE *phSharedNtHandle) {
        callCount++;
        lastObjectCount = cObjects;
        lastResourceHandle = hObjects ? *hObjects : 0u;
        lastObjectAttributes = pObjectAttributes;
        lastDesiredAccess = dwDesiredAccess;
        *phSharedNtHandle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x1234u));
        return STATUS_SUCCESS;
    }

    static void reset() {
        callCount = 0u;
        lastObjectCount = 0u;
        lastResourceHandle = 0u;
        lastObjectAttributes = nullptr;
        lastDesiredAccess = 0u;
    }

    inline static uint32_t callCount = 0u;
    inline static UINT lastObjectCount = 0u;
    inline static D3DKMT_HANDLE lastResourceHandle = 0u;
    inline static POBJECT_ATTRIBUTES lastObjectAttributes = nullptr;
    inline static DWORD lastDesiredAccess = 0u;
};

NTSTATUS APIENTRY closeAdapterForShareObjectsMock(CONST D3DKMT_CLOSEADAPTER *arg) {
    return STATUS_SUCCESS;
}
} // namespace

TEST(DrmOrWddmTest, givenWslWhenCreatingNTHandleThenResourceIsSharedWithReadAndWriteAccess) {
    MockExecutionEnvironment executionEnvironment;

    auto osEnvironment = std::make_unique<OsEnvironmentWin>();
    osEnvironment->gdi->closeAdapter = closeAdapterForShareObjectsMock;
    osEnvironment->gdi->shareObjects = ShareObjectsCapture::shareObjects;

    auto hwDeviceId = std::make_unique<HwDeviceIdWddm>(NULL_HANDLE, LUID{}, 1u, osEnvironment.get(), std::make_unique<UmKmDataTranslator>());
    auto wddm = std::make_unique<WddmMock>(std::move(hwDeviceId), *executionEnvironment.rootDeviceEnvironments[0]);

    ShareObjectsCapture::reset();

    D3DKMT_HANDLE resourceHandle = 0x40u;
    HANDLE ntHandle = nullptr;

    EXPECT_EQ(STATUS_SUCCESS, wddm->createNTHandle(&resourceHandle, &ntHandle));

    EXPECT_EQ(1u, ShareObjectsCapture::callCount);
    EXPECT_EQ(1u, ShareObjectsCapture::lastObjectCount);
    EXPECT_EQ(resourceHandle, ShareObjectsCapture::lastResourceHandle);
    EXPECT_EQ(nullptr, ShareObjectsCapture::lastObjectAttributes);
    EXPECT_EQ(0x000F0001u, ShareObjectsCapture::lastDesiredAccess);
    EXPECT_NE(static_cast<DWORD>(SHARED_ALLOCATION_WRITE), ShareObjectsCapture::lastDesiredAccess);
}
