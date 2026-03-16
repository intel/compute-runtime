/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/kernel/debug_data.h"
#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_l0_debugger.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/os_interface/linux/xe/eudebug/mock_eudebug_interface.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include <algorithm>
#include <memory>

using namespace NEO;

struct L0DebuggerSharedLinuxFixture {

    void setUp() {
        setUp(nullptr);
    }

    void setUp(HardwareInfo *hwInfo) {
        auto executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(hwInfo ? hwInfo : defaultHwInfo.get());
        executionEnvironment->initializeMemoryManager();
        auto osInterface = new OSInterface();
        drmMock = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
        executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<Drm>(drmMock));

        neoDevice.reset(NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u));

        auto rootDeviceEnvironment = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get();
        rootDeviceEnvironment->initDebuggerL0(neoDevice.get());
    }

    void tearDown() {
        drmMock = nullptr;
    };

    std::unique_ptr<NEO::MockDevice> neoDevice = nullptr;
    DrmMockResources *drmMock = nullptr;
};

using L0DebuggerSharedLinuxTest = Test<L0DebuggerSharedLinuxFixture>;

TEST_F(L0DebuggerSharedLinuxTest, givenNoOSInterfaceThenRegisterElfDoesNothing) {
    NEO::OSInterface *osInterfaceTmp = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.release();
    NEO::DebugData debugData;
    debugData.vIsa = "01234567890";
    debugData.vIsaSize = 10;
    drmMock->registeredDataSize = 0;

    neoDevice->getL0Debugger()->registerElf(&debugData);

    EXPECT_EQ(static_cast<size_t>(0u), drmMock->registeredDataSize);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(osInterfaceTmp);
}

TEST_F(L0DebuggerSharedLinuxTest, givenNoOSInterfaceThenRegisterElfAndLinkWithAllocationDoesNothing) {
    NEO::OSInterface *osInterfaceTmp = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.release();
    NEO::DebugData debugData;
    debugData.vIsa = "01234567890";
    debugData.vIsaSize = 10;
    drmMock->registeredDataSize = 0;

    MockDrmAllocation isaAllocation(neoDevice->getRootDeviceIndex(), AllocationType::kernelIsa, MemoryPool::system4KBPages);
    MockBufferObject bo(neoDevice->getRootDeviceIndex(), drmMock, 3, 0, 0, 1);
    isaAllocation.bufferObjects[0] = &bo;

    neoDevice->getL0Debugger()->registerElfAndLinkWithAllocation(&debugData, &isaAllocation);

    EXPECT_EQ(static_cast<size_t>(0u), drmMock->registeredDataSize);
    EXPECT_EQ(0u, isaAllocation.bufferObjects[0]->getBindExtHandles().size());
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(osInterfaceTmp);
}

struct SingleAddressSpaceLinuxFixture : public Test<NEO::DeviceFixture> {
    void SetUp() override {
        Test<NEO::DeviceFixture>::SetUp();
    }

    void TearDown() override {
        Test<NEO::DeviceFixture>::TearDown();
    }
};
using PlatformsSupportingSbaTracking = IsWithinGfxCore<IGFX_GEN12_CORE, IGFX_XE3_CORE>;

HWTEST2_F(SingleAddressSpaceLinuxFixture, givenDebuggingModeOfflineWhenDebuggerIsCreatedThenItHasCorrectSingleAddressSpaceValue, PlatformsSupportingSbaTracking) {
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(pDevice);
    debugger->initialize();
    EXPECT_FALSE(debugger->singleAddressSpaceSbaTracking);

    pDevice->getExecutionEnvironment()->setDebuggingMode(DebuggingMode::offline);

    debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(pDevice);
    debugger->initialize();
    EXPECT_TRUE(debugger->singleAddressSpaceSbaTracking);
}

TEST(L0DebuggerSharedLinux, givenCreateDebugDirectoryCalledWhenMkdirFailsThenReturnFalse) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsGetpid)> mockGetPid{&NEO::SysCalls::sysCallsGetpid, []() -> pid_t { return 12345; }};
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkdir)> mockMkdir{&NEO::SysCalls::sysCallsMkdir, [](const std::string &pathname) -> int {
        if (pathname == std::string(NEO::WhiteBox<NEO::DebuggerL0>::debugTmpDirPrefix) + "/12345") {
            return -1; // Simulate failure to create directory
        }
        return 0; }};

    errno = EACCES; // arbitrary mkdir errno return to simulate failure

    auto result = NEO::WhiteBox<NEO::DebuggerL0>::createDebugDirectory();
    EXPECT_FALSE(result);
}

TEST(L0DebuggerSharedLinux, givenCreateDebugDirectoryCalledWhenDirectoryExistsThenReturnTrue) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsGetpid)> mockGetPid{&NEO::SysCalls::sysCallsGetpid, []() -> pid_t { return 12345; }};
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkdir)> mockMkdir{&NEO::SysCalls::sysCallsMkdir, [](const std::string &pathname) -> int {
        if (pathname == std::string(NEO::WhiteBox<NEO::DebuggerL0>::debugTmpDirPrefix) + "/12345") {
            return -1; // Simulate failure to create directory
        }
        return 0; }};

    errno = EEXIST;

    auto result = NEO::WhiteBox<NEO::DebuggerL0>::createDebugDirectory();
    EXPECT_TRUE(result);
}

TEST(L0DebuggerSharedLinux, givenCreateDebugDirectoryCalledThenDirectoryCreatedWithCorrectPathAndReturnTrue) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsGetpid)> mockGetPid{&NEO::SysCalls::sysCallsGetpid, []() -> pid_t { return 12345; }};
    static bool debugPathSet = false;
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkdir)> mockMkdir{&NEO::SysCalls::sysCallsMkdir, [](const std::string &pathname) -> int {
       if (pathname == std::string(NEO::WhiteBox<NEO::DebuggerL0>::debugTmpDirPrefix) + "/12345") {
            debugPathSet = true;
        }
        return 0; }};

    auto result = NEO::WhiteBox<NEO::DebuggerL0>::createDebugDirectory();
    EXPECT_TRUE(result);
    EXPECT_TRUE(debugPathSet);
    debugPathSet = false;
}

TEST_F(L0DebuggerSharedLinuxTest, givenUpstreamDebuggerWhenDebuggerDestroyedThenElfLocationClearedAndRemoved) {
    struct MockIoctlHelperXeDebug : IoctlHelperXe {
        using IoctlHelperXe::euDebugInterface;
        using IoctlHelperXe::IoctlHelperXe;
    };

    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXeDebug>(*drmMock);
    xeIoctlHelper->euDebugInterface = std::make_unique<MockEuDebugInterface>();
    auto &eudebugInterface = xeIoctlHelper->euDebugInterface;
    static_cast<MockEuDebugInterface *>(eudebugInterface.get())->setCurrentInterfaceType(EuDebugInterfaceType::upstream);
    drmMock->ioctlHelper.reset(xeIoctlHelper.release());

    VariableBackup<decltype(NEO::SysCalls::sysCallsGetpid)> mockGetPid{&NEO::SysCalls::sysCallsGetpid, []() -> pid_t { return 12345; }};
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopenToNull{&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * { return nullptr; }};
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpendir)> mockOpendir{&NEO::SysCalls::sysCallsOpendir, [](const char *pathname) -> DIR * { return reinterpret_cast<DIR *>(0x5678); }};

    static struct dirent mockElfEntries[] = {
        {0, 0, 0, 0, "elf-0"},
        {0, 0, 0, 0, "elf-1"},
        {0, 0, 0, 0, "manual-entry"},
    };
    VariableBackup<decltype(NEO::SysCalls::sysCallsReaddir)> mockReaddir(
        &NEO::SysCalls::sysCallsReaddir, [](DIR *dir) -> struct dirent * {
            static int direntIndex = 0;
            if (direntIndex >= static_cast<int>(sizeof(mockElfEntries) / sizeof(dirent))) {
                direntIndex = 0;
                return nullptr;
            }
            return &mockElfEntries[direntIndex++];
        });

    static bool debugDirRemoved = false;
    VariableBackup<decltype(NEO::SysCalls::sysCallsRmdir)> mockRmDir{&NEO::SysCalls::sysCallsRmdir, [](const std::string &pathname) -> int {
        if (pathname == std::string(NEO::WhiteBox<NEO::DebuggerL0>::debugTmpDirPrefix) + "/12345") {
            debugDirRemoved = true;
        }
        return 0; }};

    NEO::WhiteBox<NEO::DebuggerL0>::initDebuggingInOs(neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.get());

    auto mockDebugger = std::make_unique<NEO::MockDebuggerL0>(neoDevice.get());
    NEO::SysCalls::rmdirCalled = 0;
    NEO::SysCalls::unlinkCalled = 0;
    mockDebugger.reset();
    EXPECT_EQ(NEO::SysCalls::rmdirCalled, 1);
    EXPECT_TRUE(debugDirRemoved);
    EXPECT_EQ(NEO::SysCalls::unlinkCalled, 3);
}

TEST_F(L0DebuggerSharedLinuxTest, givenUpstreamDebuggerWhenRemoveEmptyElfDirectoryThenDirectoryRemoved) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsGetpid)> mockGetPid{&NEO::SysCalls::sysCallsGetpid, []() -> pid_t { return 12345; }};
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpendir)> mockOpendir{&NEO::SysCalls::sysCallsOpendir, [](const char *pathname) -> DIR * { return reinterpret_cast<DIR *>(0x5678); }};

    VariableBackup<decltype(NEO::SysCalls::sysCallsReaddir)> mockReaddir{&NEO::SysCalls::sysCallsReaddir, [](DIR *dir) -> struct dirent * { return nullptr; }};

    auto mockDebugger = std::make_unique<NEO::MockDebuggerL0>(neoDevice.get());
    NEO::SysCalls::rmdirCalled = 0;
    NEO::SysCalls::unlinkCalled = 0;
    mockDebugger.reset();
    EXPECT_EQ(NEO::SysCalls::rmdirCalled, 1);
    EXPECT_EQ(NEO::SysCalls::unlinkCalled, 0);
}

TEST_F(L0DebuggerSharedLinuxTest, givenUpstreamDebuggerNotInitializedWhenDebuggerDestroyedThenRmElfLocationNotCalled) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsGetpid)> mockGetPid{&NEO::SysCalls::sysCallsGetpid, []() -> pid_t { return 12345; }};

    static bool elfDirectoryCreated = false;
    VariableBackup<decltype(NEO::SysCalls::sysCallsMkdir)> mockMkdir{&NEO::SysCalls::sysCallsMkdir, [](const std::string &pathname) -> int {
       if (pathname == std::string(NEO::WhiteBox<NEO::DebuggerL0>::debugTmpDirPrefix) + "/12345") {
            elfDirectoryCreated = true;
        }
        return 0; }};

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpendir)> mockOpendir{&NEO::SysCalls::sysCallsOpendir, [](const char *pathname) -> DIR * { return elfDirectoryCreated ? reinterpret_cast<DIR *>(0x5678) : nullptr; }};

    auto mockDebugger = std::make_unique<NEO::MockDebuggerL0>(neoDevice.get());
    NEO::SysCalls::rmdirCalled = 0;
    mockDebugger.reset();
    EXPECT_EQ(NEO::SysCalls::rmdirCalled, 0);
    EXPECT_FALSE(elfDirectoryCreated);
}
