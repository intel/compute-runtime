/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/hw_device_id.h"
#include "shared/source/os_interface/windows/os_environment_win.h"
#include "shared/source/os_interface/windows/pdh_interface.h"
#include "shared/source/os_interface/windows/sys_calls_wrapper.h"
#include "shared/source/os_interface/windows/wddm/um_km_data_translator.h"
#include "shared/source/os_interface/windows/wddm/wddm_interface.h"
#include "shared/source/os_interface/windows/wddm_memory_operations_handler.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mock_gdi/mock_gdi.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/os_interface/windows/mock_wddm_memory_manager.h"

#include "gtest/gtest.h"

#include <fcntl.h>

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

namespace {
struct ShareObjectsReturningFd {
    static NTSTATUS APIENTRY shareObjects(UINT cObjects, const D3DKMT_HANDLE *hObjects,
                                          POBJECT_ATTRIBUTES pObjectAttributes, DWORD dwDesiredAccess,
                                          HANDLE *phSharedNtHandle) {
        *phSharedNtHandle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(fdToReturn));
        return statusToReturn;
    }

    inline static int fdToReturn = 0;
    inline static NTSTATUS statusToReturn = STATUS_SUCCESS;
};

struct FcntlCapture {
    static int fcntl(int fd, int cmd, int arg) {
        callCount++;
        lastFd = fd;
        lastCmd = cmd;
        lastArg = arg;
        return fdToReturn;
    }

    inline static uint32_t callCount = 0u;
    inline static int lastFd = -1;
    inline static int lastCmd = -1;
    inline static int lastArg = -1;
    inline static int fdToReturn = -1;
};
} // namespace

class DrmOrWddmCreateNTHandleTest : public ::testing::Test {
  public:
    void SetUp() override {
        osEnvironment = std::make_unique<OsEnvironmentWin>();
        osEnvironment->gdi->closeAdapter = closeAdapterForShareObjectsMock;
        osEnvironment->gdi->shareObjects = ShareObjectsReturningFd::shareObjects;

        auto hwDeviceId = std::make_unique<HwDeviceIdWddm>(NULL_HANDLE, LUID{}, 1u, osEnvironment.get(), std::make_unique<UmKmDataTranslator>());
        wddm = std::make_unique<WddmMock>(std::move(hwDeviceId), *executionEnvironment.rootDeviceEnvironments[0]);

        FcntlCapture::fdToReturn = movedFileDescriptor;
    }

    static constexpr int movedFileDescriptor = 0x77;
    VariableBackup<int> shareObjectsFdBackup{&ShareObjectsReturningFd::fdToReturn, 0};
    VariableBackup<NTSTATUS> shareObjectsStatusBackup{&ShareObjectsReturningFd::statusToReturn, STATUS_SUCCESS};
    VariableBackup<uint32_t> fcntlCallCountBackup{&FcntlCapture::callCount, 0u};
    VariableBackup<int> fcntlFdToReturnBackup{&FcntlCapture::fdToReturn};
    VariableBackup<decltype(SysCalls::sysCallsFcntl)> sysCallsFcntlBackup{&SysCalls::sysCallsFcntl, FcntlCapture::fcntl};
    VariableBackup<uint32_t> closeFuncCalledBackup{&SysCalls::closeFuncCalled, 0u};
    VariableBackup<int> closeFuncArgPassedBackup{&SysCalls::closeFuncArgPassed, -1};
    MockExecutionEnvironment executionEnvironment;
    std::unique_ptr<OsEnvironmentWin> osEnvironment;
    std::unique_ptr<WddmMock> wddm;
    D3DKMT_HANDLE resourceHandle = 0x40u;
    HANDLE ntHandle = nullptr;
};

TEST_F(DrmOrWddmCreateNTHandleTest, givenWslWhenShareObjectsReturnsNonZeroFdThenFdIsReturnedWithoutBeingMoved) {
    ShareObjectsReturningFd::fdToReturn = 0x55;

    EXPECT_EQ(STATUS_SUCCESS, wddm->createNTHandle(&resourceHandle, &ntHandle));

    EXPECT_EQ(0x55u, castToUint64(ntHandle));
    EXPECT_EQ(0u, FcntlCapture::callCount);
    EXPECT_EQ(0u, SysCalls::closeFuncCalled);
}

TEST_F(DrmOrWddmCreateNTHandleTest, givenWslWhenShareObjectsReturnsFdZeroThenFdIsMovedToNonZeroDescriptorAndFdZeroIsClosed) {
    EXPECT_EQ(STATUS_SUCCESS, wddm->createNTHandle(&resourceHandle, &ntHandle));

    EXPECT_EQ(1u, FcntlCapture::callCount);
    EXPECT_EQ(0, FcntlCapture::lastFd);
    EXPECT_EQ(F_DUPFD_CLOEXEC, FcntlCapture::lastCmd);
    EXPECT_EQ(1, FcntlCapture::lastArg);
    EXPECT_EQ(static_cast<uint64_t>(movedFileDescriptor), castToUint64(ntHandle));
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(0, SysCalls::closeFuncArgPassed);
}

TEST_F(DrmOrWddmCreateNTHandleTest, givenWslWhenMovingFdZeroFailsThenErrorIsReturnedAndFdZeroIsClosed) {
    FcntlCapture::fdToReturn = -1;

    EXPECT_EQ(STATUS_UNSUCCESSFUL, wddm->createNTHandle(&resourceHandle, &ntHandle));

    EXPECT_EQ(1u, FcntlCapture::callCount);
    EXPECT_EQ(nullptr, ntHandle);
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(0, SysCalls::closeFuncArgPassed);
}

TEST_F(DrmOrWddmCreateNTHandleTest, givenWslWhenShareObjectsFailsThenErrorIsReturnedAndNoFdIsMovedOrClosed) {
    ShareObjectsReturningFd::statusToReturn = STATUS_UNSUCCESSFUL;

    EXPECT_EQ(STATUS_UNSUCCESSFUL, wddm->createNTHandle(&resourceHandle, &ntHandle));

    EXPECT_EQ(0u, FcntlCapture::callCount);
    EXPECT_EQ(0u, SysCalls::closeFuncCalled);
}

TEST(DrmOrWddmTest, givenWslWhenClosingNullHandleThenSuccessIsReturnedAndNoFileDescriptorIsClosed) {
    VariableBackup<uint32_t> closeFuncCalledBackup(&SysCalls::closeFuncCalled, 0u);

    EXPECT_EQ(TRUE, SysCalls::closeHandle(nullptr));
    EXPECT_EQ(0u, SysCalls::closeFuncCalled);
}

TEST(DrmOrWddmTest, givenWslWhenClosingHandleThenFileDescriptorIsClosedAndResultsAreCorrect) {
    VariableBackup<uint32_t> closeFuncCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<int> closeFuncArgPassedBackup(&SysCalls::closeFuncArgPassed, 0);
    VariableBackup<int> closeFuncRetValBackup(&SysCalls::closeFuncRetVal, 0);

    constexpr int fileDescriptor = 0x55;
    auto handle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(fileDescriptor));

    EXPECT_EQ(TRUE, SysCalls::closeHandle(handle));
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(fileDescriptor, SysCalls::closeFuncArgPassed);

    SysCalls::closeFuncRetVal = -1;
    EXPECT_EQ(FALSE, SysCalls::closeHandle(handle));
    EXPECT_EQ(2u, SysCalls::closeFuncCalled);
}

class DrmOrWddmMemoryManagerTest : public ::testing::Test {
  public:
    void SetUp() override {
        auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
        rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
        wddm = new WddmMock(rootDeviceEnvironment);
        rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
        rootDeviceEnvironment.memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
        wddm->init();
        memoryManager = std::make_unique<MockWddmMemoryManager>(executionEnvironment);
    }

    WddmAllocation *createShareableAllocationWithNtHandle() {
        auto allocation = new WddmAllocation(0u, 1u, AllocationType::buffer, nullptr, 0u, MemoryConstants::pageSize,
                                             nullptr, MemoryPool::system4KBPages, 1u, 1u);
        allocation->getHandleToModify(0u) = ALLOCATION_HANDLE;
        *allocation->getSharedHandleToModify() = ntHandleFileDescriptor;
        return allocation;
    }

    static constexpr int ntHandleFileDescriptor = 0x55;
    VariableBackup<uint32_t> closeFuncCalledBackup{&SysCalls::closeFuncCalled, 0u};
    VariableBackup<int> closeFuncArgPassedBackup{&SysCalls::closeFuncArgPassed, 0};
    MockExecutionEnvironment executionEnvironment;
    WddmMock *wddm = nullptr;
    std::unique_ptr<MockWddmMemoryManager> memoryManager;
};

TEST_F(DrmOrWddmMemoryManagerTest, givenWslShareableAllocationWithNtHandleWhenFreeingAllocationThenNtHandleFileDescriptorIsClosed) {
    auto allocation = createShareableAllocationWithNtHandle();

    memoryManager->freeGraphicsMemory(allocation);

    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(ntHandleFileDescriptor, SysCalls::closeFuncArgPassed);
}
