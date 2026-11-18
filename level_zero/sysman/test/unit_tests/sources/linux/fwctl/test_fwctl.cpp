/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/linux/fwctl/mock_fwctl.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"

#include <map>

namespace L0 {
namespace Sysman {
namespace ult {

const std::map<int, ze_result_t> errnoToSysmanErrorMap = {
    {EPERM, ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS},
    {EACCES, ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS},
    {ENOENT, ZE_RESULT_ERROR_NOT_AVAILABLE},
    {EBUSY, ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE},
    {ENOTTY, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE},
    {EOPNOTSUPP, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE},
    {E2BIG, ZE_RESULT_ERROR_INVALID_SIZE},
    {EMSGSIZE, ZE_RESULT_ERROR_INVALID_SIZE},
    {EINVAL, ZE_RESULT_ERROR_INVALID_ARGUMENT},
    {EBADMSG, ZE_RESULT_ERROR_INVALID_ARGUMENT},
    {ENOMEM, ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY},
    {ENODEV, ZE_RESULT_ERROR_DEVICE_LOST},
    {EIO, ZE_RESULT_ERROR_UNKNOWN}};

struct SysmanFwctlFixture : public SysmanDeviceFixture {
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        resetMockFwctlSysCalls();
        pFwctl = FwctlInterface::create(pLinuxSysmanImp);
    }
    void TearDown() override {
        pFwctl.reset();
        clearMockFwctlSysCalls();
        SysmanDeviceFixture::TearDown();
    }

    void backupDirectorySysCalls() {
        opendirBackup = std::make_unique<VariableBackup<decltype(NEO::SysCalls::sysCallsOpendir)>>(&NEO::SysCalls::sysCallsOpendir, &mockFwctlOpendir);
        readdirBackup = std::make_unique<VariableBackup<decltype(NEO::SysCalls::sysCallsReaddir)>>(&NEO::SysCalls::sysCallsReaddir, &mockFwctlReaddir);
        closedirBackup = std::make_unique<VariableBackup<decltype(NEO::SysCalls::sysCallsClosedir)>>(&NEO::SysCalls::sysCallsClosedir, &mockFwctlClosedir);
    }

    static fwctl_rpc_xe_sysctrl eccStatusRequest() {
        fwctl_rpc_xe_sysctrl rpc = {};
        rpc.op = FWCTL_XE_SYSCTRL_OP_ECC_STATUS_GET;
        return rpc;
    }

    ze_result_t sendEccStatusRpc(fwctl_rpc_xe_sysctrl &rpc) {
        return pFwctl->sendSysctrlRpc(FWCTL_XE_SYSCTRL_CAP_ECC, FWCTL_RPC_CONFIGURATION, rpc);
    }

  protected:
    std::unique_ptr<FwctlInterface> pFwctl = nullptr;
    std::unique_ptr<VariableBackup<decltype(NEO::SysCalls::sysCallsOpendir)>> opendirBackup;
    std::unique_ptr<VariableBackup<decltype(NEO::SysCalls::sysCallsReaddir)>> readdirBackup;
    std::unique_ptr<VariableBackup<decltype(NEO::SysCalls::sysCallsClosedir)>> closedirBackup;
};

TEST_F(SysmanFwctlFixture, GivenValidCallWhenCreatingFwctlInterfaceThenNonNullInterfaceIsReturned) {
    auto fwctlInterface = FwctlInterface::create(pLinuxSysmanImp);
    EXPECT_NE(fwctlInterface, nullptr);
}

TEST_F(SysmanFwctlFixture, GivenSysctrlNodeIsPresentWhenSendingRpcThenNodeIsDiscoveredFromDeviceBdfAndRequestIsIssuedAsTheUapiRequires) {
    backupDirectorySysCalls();
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockFwctlOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockFwctlCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, &mockFwctlIoctlSuccess);

    mockFwctlSysCalls.responseData0 = 1u;
    mockFwctlSysCalls.responseData1 = 1u;

    auto rpc = eccStatusRequest();
    EXPECT_EQ(ZE_RESULT_SUCCESS, sendEccStatusRpc(rpc));

    const std::string expectedDirectory = "/sys/bus/pci/devices/" + pLinuxSysmanImp->getDevicePciBdf() + "/fwctl";
    EXPECT_EQ(expectedDirectory, mockFwctlSysCalls.openedDirectoryPath);

    EXPECT_EQ(static_cast<uint32_t>(sizeof(fwctl_info)), mockFwctlSysCalls.capturedInfoSize);
    EXPECT_EQ(0u, mockFwctlSysCalls.capturedInfoFlags);
    EXPECT_EQ(static_cast<uint32_t>(sizeof(fwctl_info_xe_sysctrl)), mockFwctlSysCalls.capturedInfoDeviceDataLen);

    EXPECT_EQ(1u, mockFwctlSysCalls.rpcCount);
    EXPECT_EQ(static_cast<uint32_t>(sizeof(fwctl_rpc)), mockFwctlSysCalls.capturedRpcSize);
    EXPECT_EQ(static_cast<uint32_t>(FWCTL_RPC_CONFIGURATION), mockFwctlSysCalls.capturedRpcScope);
    EXPECT_EQ(static_cast<uint32_t>(sizeof(fwctl_rpc_xe_sysctrl)), mockFwctlSysCalls.capturedRpcInLen);
    EXPECT_EQ(static_cast<uint32_t>(sizeof(fwctl_rpc_xe_sysctrl)), mockFwctlSysCalls.capturedRpcOutLen);
    EXPECT_TRUE(mockFwctlSysCalls.capturedRpcInAliasesOut);
    EXPECT_EQ(static_cast<uint32_t>(FWCTL_XE_SYSCTRL_OP_ECC_STATUS_GET), mockFwctlSysCalls.capturedRpcOp);
    EXPECT_EQ(0u, mockFwctlSysCalls.capturedRpcFlags);

    EXPECT_EQ(1u, rpc.data[0]);
    EXPECT_EQ(1u, rpc.data[1]);
}

TEST_F(SysmanFwctlFixture, GivenANodeOfAnotherDeviceTypeIsListedFirstWhenSendingRpcThenItIsSkippedAndTheSysctrlNodeIsUsed) {
    backupDirectorySysCalls();
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockFwctlOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockFwctlCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, &mockFwctlIoctlSuccess);

    auto rpc = eccStatusRequest();
    EXPECT_EQ(ZE_RESULT_SUCCESS, sendEccStatusRpc(rpc));

    ASSERT_EQ(2u, mockFwctlSysCalls.openedPaths.size());
    EXPECT_EQ(mockFwctlNonSysctrlDevicePath, mockFwctlSysCalls.openedPaths[0]);
    EXPECT_EQ(mockFwctlSysctrlDevicePath, mockFwctlSysCalls.openedPaths[1]);
    EXPECT_EQ(2u, mockFwctlSysCalls.infoCount);
    EXPECT_EQ(1u, mockFwctlSysCalls.rpcCount);
}

TEST_F(SysmanFwctlFixture, GivenOnlyNodesOfAnotherDeviceTypeArePresentWhenSendingRpcThenUnsupportedFeatureIsReturnedAndNoRpcIsIssued) {
    mockFwctlSysCalls.nodeNames = {mockFwctlNonSysctrlNodeName};
    backupDirectorySysCalls();
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockFwctlOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockFwctlCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, &mockFwctlIoctlSuccess);

    auto rpc = eccStatusRequest();
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, sendEccStatusRpc(rpc));
    EXPECT_EQ(0u, mockFwctlSysCalls.rpcCount);
    EXPECT_EQ(mockFwctlSysCalls.openCount, mockFwctlSysCalls.closeCount);
}

TEST_F(SysmanFwctlFixture, GivenFwctlDirectoryIsAbsentWhenSendingRpcThenUnsupportedFeatureIsReturned) {
    mockFwctlSysCalls.opendirFails = true;
    backupDirectorySysCalls();
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockFwctlOpenSuccess);

    auto rpc = eccStatusRequest();
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, sendEccStatusRpc(rpc));
    EXPECT_EQ(0u, mockFwctlSysCalls.openCount);
}

TEST_F(SysmanFwctlFixture, GivenFwctlDirectoryCannotBeListedForAnotherReasonWhenSendingRpcThenTheErrorIsReturnedUnchanged) {
    mockFwctlSysCalls.opendirFails = true;
    mockFwctlSysCalls.opendirErrno = EACCES;
    backupDirectorySysCalls();
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockFwctlOpenSuccess);

    auto rpc = eccStatusRequest();
    EXPECT_EQ(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS, sendEccStatusRpc(rpc));
    EXPECT_EQ(0u, mockFwctlSysCalls.openCount);
}

TEST_F(SysmanFwctlFixture, GivenDevicePciPathIsEmptyWhenSendingRpcThenUnsupportedFeatureIsReturnedAndNoNodeIsOpened) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsRealpath)> mockRealPath(&NEO::SysCalls::sysCallsRealpath, [](const char *path, char *buf) -> char * {
        return nullptr;
    });
    auto pSysfsAccessWithoutBdf = SysFsAccessInterface::create("/dev/dri/card0");
    VariableBackup<SysFsAccessInterface *> sysfsAccessBackup(&pLinuxSysmanImp->pSysfsAccess, pSysfsAccessWithoutBdf.get());

    backupDirectorySysCalls();
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockFwctlOpenSuccess);

    auto rpc = eccStatusRequest();
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, sendEccStatusRpc(rpc));
    EXPECT_TRUE(mockFwctlSysCalls.openedDirectoryPath.empty());
    EXPECT_EQ(0u, mockFwctlSysCalls.openCount);
}

TEST_F(SysmanFwctlFixture, GivenFwctlDirectoryIsEmptyWhenSendingRpcThenUnsupportedFeatureIsReturned) {
    mockFwctlSysCalls.nodeNames.clear();
    backupDirectorySysCalls();
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockFwctlOpenSuccess);

    auto rpc = eccStatusRequest();
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, sendEccStatusRpc(rpc));
    EXPECT_EQ(0u, mockFwctlSysCalls.openCount);
}

TEST_F(SysmanFwctlFixture, GivenOpenOfTheNodeFailsWhenSendingRpcThenErrnoIsMappedToTheExpectedError) {
    static thread_local int currentErrno = 0;

    backupDirectorySysCalls();
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        errno = currentErrno;
        return -1;
    });

    size_t iterationCount = 0;
    for (const auto &[errnoValue, expectedSysmanError] : errnoToSysmanErrorMap) {
        currentErrno = errnoValue;
        iterationCount++;

        auto rpc = eccStatusRequest();
        EXPECT_EQ(expectedSysmanError, sendEccStatusRpc(rpc));
    }

    EXPECT_EQ(iterationCount, errnoToSysmanErrorMap.size());
}

TEST_F(SysmanFwctlFixture, GivenInfoIoctlFailsWhenSendingRpcThenTheNodeIsSkippedAndItsDescriptorIsClosed) {
    mockFwctlSysCalls.nodeNames = {mockFwctlSysctrlNodeName};
    backupDirectorySysCalls();
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockFwctlOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockFwctlCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, [](int fd, unsigned long request, void *arg) -> int {
        if (request == fwctlInfoCmd) {
            errno = ENOTTY;
            return -1;
        }
        return -1;
    });

    auto rpc = eccStatusRequest();
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, sendEccStatusRpc(rpc));
    EXPECT_EQ(0u, mockFwctlSysCalls.rpcCount);
    EXPECT_EQ(1u, mockFwctlSysCalls.openCount);
    EXPECT_EQ(1u, mockFwctlSysCalls.closeCount);
}

TEST_F(SysmanFwctlFixture, GivenRequiredCapabilityIsNotReportedWhenSendingRpcThenUnsupportedFeatureIsReturnedAndNoRpcIsIssued) {
    mockFwctlSysCalls.uctxCaps = FWCTL_XE_SYSCTRL_CAP_RAS_ERROR_INJECT;
    backupDirectorySysCalls();
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockFwctlOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockFwctlCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, &mockFwctlIoctlSuccess);

    auto rpc = eccStatusRequest();
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, sendEccStatusRpc(rpc));
    EXPECT_EQ(0u, mockFwctlSysCalls.rpcCount);
    EXPECT_EQ(mockFwctlSysCalls.openCount, mockFwctlSysCalls.closeCount);
}

TEST_F(SysmanFwctlFixture, GivenRpcIoctlFailsWhenSendingRpcThenErrnoIsMappedToTheExpectedErrorAndDescriptorIsClosed) {
    static thread_local int currentErrno = 0;

    mockFwctlSysCalls.nodeNames = {mockFwctlSysctrlNodeName};
    backupDirectorySysCalls();
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockFwctlOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockFwctlCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, [](int fd, unsigned long request, void *arg) -> int {
        if (request == fwctlRpcCmd) {
            mockFwctlSysCalls.rpcCount++;
            errno = currentErrno;
            return -1;
        }
        return mockFwctlIoctlSuccess(fd, request, arg);
    });

    size_t iterationCount = 0;
    for (const auto &[errnoValue, expectedSysmanError] : errnoToSysmanErrorMap) {
        currentErrno = errnoValue;
        iterationCount++;

        auto rpc = eccStatusRequest();
        EXPECT_EQ(expectedSysmanError, sendEccStatusRpc(rpc));
    }

    EXPECT_EQ(iterationCount, errnoToSysmanErrorMap.size());
    EXPECT_EQ(iterationCount, mockFwctlSysCalls.rpcCount);
    EXPECT_EQ(mockFwctlSysCalls.openCount, mockFwctlSysCalls.closeCount);
}

TEST_F(SysmanFwctlFixture, GivenRpcSucceedsWhenSendingRpcThenTheDescriptorIsReleasedBeforeReturning) {
    backupDirectorySysCalls();
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockFwctlOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockFwctlCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, &mockFwctlIoctlSuccess);

    auto rpc = eccStatusRequest();
    EXPECT_EQ(ZE_RESULT_SUCCESS, sendEccStatusRpc(rpc));
    EXPECT_EQ(mockFwctlSysCalls.openCount, mockFwctlSysCalls.closeCount);
}

TEST_F(SysmanFwctlFixture, GivenFirmwareReportsEccSupportedButDisabledWhenSendingEccStatusRpcThenBothResponseWordsAreDistinguished) {
    backupDirectorySysCalls();
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockFwctlOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockFwctlCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, &mockFwctlIoctlSuccess);

    mockFwctlSysCalls.responseData0 = 1u;
    mockFwctlSysCalls.responseData1 = 0u;

    auto rpc = eccStatusRequest();
    EXPECT_EQ(ZE_RESULT_SUCCESS, sendEccStatusRpc(rpc));
    EXPECT_EQ(1u, rpc.data[0]);
    EXPECT_EQ(0u, rpc.data[1]);
}

TEST_F(SysmanFwctlFixture, GivenSysctrlNodeWasDiscoveredWhenSendingRpcAgainThenTheCachedNodeIsOpenedWithoutDiscovery) {
    backupDirectorySysCalls();
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockFwctlOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockFwctlCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, &mockFwctlIoctlSuccess);

    auto rpc = eccStatusRequest();
    EXPECT_EQ(ZE_RESULT_SUCCESS, sendEccStatusRpc(rpc));
    EXPECT_EQ(2u, mockFwctlSysCalls.infoCount);

    mockFwctlSysCalls.openedDirectoryPath.clear();
    rpc = eccStatusRequest();
    EXPECT_EQ(ZE_RESULT_SUCCESS, sendEccStatusRpc(rpc));

    EXPECT_TRUE(mockFwctlSysCalls.openedDirectoryPath.empty());
    EXPECT_EQ(2u, mockFwctlSysCalls.infoCount);
    ASSERT_EQ(3u, mockFwctlSysCalls.openedPaths.size());
    EXPECT_EQ(mockFwctlSysctrlDevicePath, mockFwctlSysCalls.openedPaths[2]);
    EXPECT_EQ(2u, mockFwctlSysCalls.rpcCount);
    EXPECT_EQ(mockFwctlSysCalls.openCount, mockFwctlSysCalls.closeCount);
}

TEST_F(SysmanFwctlFixture, GivenCachedNodeCannotBeOpenedWhenSendingRpcThenErrorIsReturnedAndTheNodeIsDiscoveredAgainOnTheNextCall) {
    backupDirectorySysCalls();
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockFwctlCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, &mockFwctlIoctlSuccess);

    auto rpc = eccStatusRequest();
    {
        VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockFwctlOpenSuccess);
        EXPECT_EQ(ZE_RESULT_SUCCESS, sendEccStatusRpc(rpc));
    }
    EXPECT_EQ(2u, mockFwctlSysCalls.infoCount);

    {
        VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
            errno = ENODEV;
            return -1;
        });
        rpc = eccStatusRequest();
        EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, sendEccStatusRpc(rpc));
    }
    EXPECT_EQ(2u, mockFwctlSysCalls.infoCount);

    {
        VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockFwctlOpenSuccess);
        rpc = eccStatusRequest();
        EXPECT_EQ(ZE_RESULT_SUCCESS, sendEccStatusRpc(rpc));
    }
    EXPECT_EQ(4u, mockFwctlSysCalls.infoCount);
    EXPECT_EQ(2u, mockFwctlSysCalls.rpcCount);
}

TEST_F(SysmanFwctlFixture, GivenRpcFailsWithNoDeviceWhenSendingRpcThenTheNodeIsDiscoveredAgainOnTheNextCall) {
    static thread_local bool failRpc = false;

    backupDirectorySysCalls();
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockFwctlOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockFwctlCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, [](int fd, unsigned long request, void *arg) -> int {
        if (failRpc && (request == fwctlRpcCmd)) {
            errno = ENODEV;
            return -1;
        }
        return mockFwctlIoctlSuccess(fd, request, arg);
    });

    failRpc = false;
    auto rpc = eccStatusRequest();
    EXPECT_EQ(ZE_RESULT_SUCCESS, sendEccStatusRpc(rpc));
    EXPECT_EQ(2u, mockFwctlSysCalls.infoCount);

    failRpc = true;
    rpc = eccStatusRequest();
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, sendEccStatusRpc(rpc));
    EXPECT_EQ(2u, mockFwctlSysCalls.infoCount);

    failRpc = false;
    rpc = eccStatusRequest();
    EXPECT_EQ(ZE_RESULT_SUCCESS, sendEccStatusRpc(rpc));
    EXPECT_EQ(4u, mockFwctlSysCalls.infoCount);
    EXPECT_EQ(mockFwctlSysCalls.openCount, mockFwctlSysCalls.closeCount);
}

TEST_F(SysmanFwctlFixture, GivenRpcFailsWithAnErrorOtherThanNoDeviceWhenSendingRpcThenTheCachedNodeIsKept) {
    static thread_local bool failRpc = false;

    backupDirectorySysCalls();
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, &mockFwctlOpenSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> mockClose(&NEO::SysCalls::sysCallsClose, &mockFwctlCloseSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsIoctl)> mockIoctl(&NEO::SysCalls::sysCallsIoctl, [](int fd, unsigned long request, void *arg) -> int {
        if (failRpc && (request == fwctlRpcCmd)) {
            errno = EIO;
            return -1;
        }
        return mockFwctlIoctlSuccess(fd, request, arg);
    });

    failRpc = false;
    auto rpc = eccStatusRequest();
    EXPECT_EQ(ZE_RESULT_SUCCESS, sendEccStatusRpc(rpc));

    failRpc = true;
    rpc = eccStatusRequest();
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, sendEccStatusRpc(rpc));

    failRpc = false;
    rpc = eccStatusRequest();
    EXPECT_EQ(ZE_RESULT_SUCCESS, sendEccStatusRpc(rpc));
    EXPECT_EQ(2u, mockFwctlSysCalls.infoCount);
}

TEST_F(SysmanFwctlFixture, GivenFwctlInterfaceIsRequestedTwiceFromLinuxSysmanImpThenTheSameInstanceIsReturned) {
    auto pFirst = pLinuxSysmanImp->getFwctlInterface();
    EXPECT_NE(nullptr, pFirst);
    EXPECT_EQ(pFirst, pLinuxSysmanImp->getFwctlInterface());
}

TEST_F(SysmanFwctlFixture, GivenFwctlInterfaceWasCreatedWhenSysmanDeviceResourcesAreReleasedThenItIsReleasedAndCreatedAgainOnTheNextRequest) {
    EXPECT_NE(nullptr, pLinuxSysmanImp->getFwctlInterface());

    pLinuxSysmanImp->diagnosticsReset = true;
    pLinuxSysmanImp->releaseSysmanDeviceResources();
    EXPECT_EQ(nullptr, pLinuxSysmanImp->pFwctlInterface.get());

    EXPECT_EQ(ZE_RESULT_SUCCESS, pLinuxSysmanImp->reInitSysmanDeviceResources());
    EXPECT_EQ(nullptr, pLinuxSysmanImp->pFwctlInterface.get());
    EXPECT_NE(nullptr, pLinuxSysmanImp->getFwctlInterface());
}

} // namespace ult
} // namespace Sysman
} // namespace L0
