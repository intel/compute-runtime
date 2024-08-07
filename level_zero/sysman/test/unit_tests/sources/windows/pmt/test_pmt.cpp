/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/sysman/source/shared/windows/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/pmt/mock_pmt.h"

namespace L0 {
namespace Sysman {
namespace ult {

const std::wstring interfaceName = L"TEST\0";
std::vector<wchar_t> deviceInterface(interfaceName.begin(), interfaceName.end());

const std::map<std::string, std::pair<uint32_t, uint32_t>> dummyKeyOffsetMap = {
    {"DUMMY_KEY", {0x0, 1}}};

inline static HANDLE mockCreateFileSuccess(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x7));
}

inline static BOOL mockDeviceIoControlSuccess(HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) {
    *lpBytesReturned = 4;
    *static_cast<int *>(lpOutBuffer) = 1;
    return true;
}

class SysmanDevicePmtFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<PublicPlatformMonitoringTech> pPmt;
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        std::vector<wchar_t> deviceInterface;
        pPmt = std::make_unique<PublicPlatformMonitoringTech>(deviceInterface, pWddmSysmanImp->getSysmanProductHelper());
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(SysmanDevicePmtFixture, GivenWrongKeyWhenCallingReadValueWithUint32TypeThenCallFails) {

    uint32_t val = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pPmt->readValue("SOMETHING", val));
}

TEST_F(SysmanDevicePmtFixture, GivenWrongKeyWhenCallingReadValueWithUint64TypeThenCallFails) {

    uint64_t val = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pPmt->readValue("SOMETHING", val));
}

TEST_F(SysmanDevicePmtFixture, GivenNullPmtHandleWhenCallingReadValueWithUint32CallFails) {
    uint32_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pPmt->readValue("DUMMY_KEY", val));
}

TEST_F(SysmanDevicePmtFixture, GivenInvalidPmtHandleWhenCallingReadValue32CallFails) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, [](LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) -> HANDLE {
        return INVALID_HANDLE_VALUE;
    });
    pPmt = std::make_unique<PublicPlatformMonitoringTech>(deviceInterface, pWddmSysmanImp->getSysmanProductHelper());
    uint32_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pPmt->readValue("DUMMY_KEY", val));
}

TEST_F(SysmanDevicePmtFixture, GivenInvalidPmtHandleWhenCallingReadValue64CallFails) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, [](LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) -> HANDLE {
        return INVALID_HANDLE_VALUE;
    });
    pPmt = std::make_unique<PublicPlatformMonitoringTech>(deviceInterface, pWddmSysmanImp->getSysmanProductHelper());
    uint64_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pPmt->readValue("DUMMY_KEY", val));
}

TEST_F(SysmanDevicePmtFixture, GivenValidPmtHandleWhenCallingReadValueWithUint32WhenIoctlCallFailsThenProperErrorIsReturned) {
    pPmt = std::make_unique<PublicPlatformMonitoringTech>(deviceInterface, pWddmSysmanImp->getSysmanProductHelper());
    pPmt->pcreateFile = mockCreateFileSuccess;
    uint32_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pPmt->readValue("DUMMY_KEY", val));
}

TEST_F(SysmanDevicePmtFixture, GivenValidPmtHandleWhenCallingReadValueWithUint32TypeWhenIoctlCallSucceedsThenreadValueReturned) {
    pPmt = std::make_unique<PublicPlatformMonitoringTech>(deviceInterface, pWddmSysmanImp->getSysmanProductHelper());
    pPmt->pcreateFile = mockCreateFileSuccess;
    pPmt->pdeviceIoControl = mockDeviceIoControlSuccess;

    uint32_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pPmt->readValue("DUMMY_KEY", val));
    EXPECT_NE(0u, val);
}

TEST_F(SysmanDevicePmtFixture, GivenValidPmtHandleWhenCallingReadValueWithUint32WithNullBytesCountReturnedFromIoctlCallThenreadValueFails) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        *lpBytesReturned = 0;
        *static_cast<int *>(lpOutBuffer) = 4;
        return true;
    });
    pPmt = std::make_unique<PublicPlatformMonitoringTech>(deviceInterface, pWddmSysmanImp->getSysmanProductHelper());
    pPmt->pcreateFile = mockCreateFileSuccess;

    uint32_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pPmt->readValue("DUMMY_KEY", val));
}

TEST_F(SysmanDevicePmtFixture, GivenValidPmtHandleWhenCallingReadValueWithUint64WithNullBytesCountReturnedFromIoctlCallThenreadValueFails) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        *lpBytesReturned = 0;
        *static_cast<int *>(lpOutBuffer) = 4;
        return true;
    });
    pPmt = std::make_unique<PublicPlatformMonitoringTech>(deviceInterface, pWddmSysmanImp->getSysmanProductHelper());
    pPmt->pcreateFile = mockCreateFileSuccess;

    uint64_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pPmt->readValue("DUMMY_KEY", val));
}

TEST_F(SysmanDevicePmtFixture, GivenValidPmtHandleWhenCallingreadValueWithUint64TypeAndIoctlCallSucceedsThenreadValueReturned) {
    pPmt = std::make_unique<PublicPlatformMonitoringTech>(deviceInterface, pWddmSysmanImp->getSysmanProductHelper());
    pPmt->pcreateFile = mockCreateFileSuccess;
    pPmt->pdeviceIoControl = mockDeviceIoControlSuccess;
    uint64_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pPmt->readValue("DUMMY_KEY", val));
    EXPECT_NE(0u, val);
}

TEST_F(SysmanDevicePmtFixture, GivenInvalidPmtInterfaceWhenCallingCreateThenCallReturnsNullPtr) {
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper());
    EXPECT_EQ(nullptr, pPmt);
}

TEST_F(SysmanDevicePmtFixture, GivenInvalidPmtInterfaceWhenCallingGetSysmanPmtThenCallReturnsNullPtr) {
    PlatformMonitoringTech *pPmt = pWddmSysmanImp->getSysmanPmt();
    EXPECT_EQ(nullptr, pPmt);
}

TEST_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWithUnsupportedGuidWhenCallingGetSysmanPmtThenCallReturnsNullPtr) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, [](LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) -> HANDLE {
        return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x7));
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        if (static_cast<uint32_t>(dwIoControlCode) == PmtSysman::IoctlPmtGetTelemetryDiscoverySize) {
            *static_cast<unsigned long *>(lpOutBuffer) = 40;
        } else if (static_cast<uint32_t>(dwIoControlCode) == PmtSysman::IoctlPmtGetTelemetryDiscovery) {
            PmtSysman::PmtTelemetryDiscovery temp = {1, 2, {{1, 1, 0x5e2f8210, 10}}};
            *static_cast<PmtSysman::PmtTelemetryDiscovery *>(lpOutBuffer) = temp;
        }
        return true;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, [](PULONG pulLen, LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, ULONG ulFlags) -> CONFIGRET {
        *pulLen = 4;
        return CR_SUCCESS;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, [](LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, PZZWSTR buffer, ULONG bufferLen, ULONG ulFlags) -> CONFIGRET {
        return CR_SUCCESS;
    });
    struct MockSysmanProductHelperPmt : L0::Sysman::SysmanProductHelperHw<IGFX_UNKNOWN> {
        MockSysmanProductHelperPmt() = default;
    };

    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper = std::make_unique<MockSysmanProductHelperPmt>();
    std::swap(pWddmSysmanImp->pSysmanProductHelper, pSysmanProductHelper);
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper());
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateThenCallSucceeds, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, [](LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) -> HANDLE {
        return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x7));
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        if (static_cast<uint32_t>(dwIoControlCode) == PmtSysman::IoctlPmtGetTelemetryDiscoverySize) {
            *static_cast<unsigned long *>(lpOutBuffer) = 40;
        } else if (static_cast<uint32_t>(dwIoControlCode) == PmtSysman::IoctlPmtGetTelemetryDiscovery) {
            PmtSysman::PmtTelemetryDiscovery temp = {1, 2, {{1, 1, 0x5e2f8210, 10}}};
            *static_cast<PmtSysman::PmtTelemetryDiscovery *>(lpOutBuffer) = temp;
        }
        return true;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, [](PULONG pulLen, LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, ULONG ulFlags) -> CONFIGRET {
        *pulLen = 4;
        return CR_SUCCESS;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, [](LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, PZZWSTR buffer, ULONG bufferLen, ULONG ulFlags) -> CONFIGRET {
        return CR_SUCCESS;
    });
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper());
    EXPECT_NE(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateWithNoGuidsFoundThenCallFails, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, [](LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) -> HANDLE {
        return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x7));
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        if (static_cast<uint32_t>(dwIoControlCode) == PmtSysman::IoctlPmtGetTelemetryDiscoverySize) {
            *static_cast<unsigned long *>(lpOutBuffer) = 40;
        }
        return true;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, [](PULONG pulLen, LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, ULONG ulFlags) -> CONFIGRET {
        *pulLen = 4;
        return CR_SUCCESS;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, [](LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, PZZWSTR buffer, ULONG bufferLen, ULONG ulFlags) -> CONFIGRET {
        return CR_SUCCESS;
    });
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper());
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateAndTelemtrySizeIsZeroThenCallFails, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, [](LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) -> HANDLE {
        return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x7));
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        if (static_cast<uint32_t>(dwIoControlCode) == PmtSysman::IoctlPmtGetTelemetryDiscoverySize) {
            *static_cast<unsigned long *>(lpOutBuffer) = 0;
        }
        return true;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, [](PULONG pulLen, LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, ULONG ulFlags) -> CONFIGRET {
        *pulLen = 4;
        return CR_SUCCESS;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, [](LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, PZZWSTR buffer, ULONG bufferLen, ULONG ulFlags) -> CONFIGRET {
        return CR_SUCCESS;
    });
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper());
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateAndTelemtryDiscoveryFailsThenCallFails, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, [](LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) -> HANDLE {
        return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x7));
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        if (static_cast<uint32_t>(dwIoControlCode) == PmtSysman::IoctlPmtGetTelemetryDiscoverySize) {
            *static_cast<unsigned long *>(lpOutBuffer) = 40;
        } else if (static_cast<uint32_t>(dwIoControlCode) == PmtSysman::IoctlPmtGetTelemetryDiscovery) {
            return false;
        }
        return true;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, [](PULONG pulLen, LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, ULONG ulFlags) -> CONFIGRET {
        *pulLen = 4;
        return CR_SUCCESS;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, [](LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, PZZWSTR buffer, ULONG bufferLen, ULONG ulFlags) -> CONFIGRET {
        return CR_SUCCESS;
    });
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper());
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateAndTelemtryDiscoveryFailsThenCallFailsCoveringNewBranch, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, [](LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) -> HANDLE {
        return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x7));
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, [](PULONG pulLen, LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, ULONG ulFlags) -> CONFIGRET {
        *pulLen = 4;
        return CR_SUCCESS;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, [](LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, PZZWSTR buffer, ULONG bufferLen, ULONG ulFlags) -> CONFIGRET {
        return CR_SUCCESS;
    });
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper());
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateThenCallSucceedsCoveringNewBranch, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, [](LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) -> HANDLE {
        return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x7));
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        if (static_cast<uint32_t>(dwIoControlCode) == PmtSysman::IoctlPmtGetTelemetryDiscoverySize) {
            *static_cast<unsigned long *>(lpOutBuffer) = 40;
        } else if (static_cast<uint32_t>(dwIoControlCode) == PmtSysman::IoctlPmtGetTelemetryDiscovery) {
            PmtSysman::PmtTelemetryDiscovery temp = {1, 2, {{1, 1, 0x5e2f8210, 10}}};
            *static_cast<PmtSysman::PmtTelemetryDiscovery *>(lpOutBuffer) = temp;
        }
        return true;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, [](PULONG pulLen, LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, ULONG ulFlags) -> CONFIGRET {
        *pulLen = 4;
        return CR_SUCCESS;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, [](LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, PZZWSTR buffer, ULONG bufferLen, ULONG ulFlags) -> CONFIGRET {
        static int sysCallsInterfaceListCounter = 0;
        if (sysCallsInterfaceListCounter) {
            return CR_SUCCESS;
        }
        sysCallsInterfaceListCounter++;
        return CR_BUFFER_SMALL;
    });
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper());
    EXPECT_NE(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenInValidPmtInterfaceWhenCallingCreateThenCallFails, IsBMG) {
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper());
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateAndCmGetDeviceInterfaceListFailsThenCallFails, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, [](PULONG pulLen, LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, ULONG ulFlags) -> CONFIGRET {
        *pulLen = 4;
        return CR_SUCCESS;
    });
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper());
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateAndCmGetDeviceInterfaceListSizeReturnZeroThenCallFails, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, [](PULONG pulLen, LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, ULONG ulFlags) -> CONFIGRET {
        *pulLen = 0;
        return CR_SUCCESS;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, [](LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, PZZWSTR buffer, ULONG bufferLen, ULONG ulFlags) -> CONFIGRET {
        return CR_SUCCESS;
    });
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper());
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateAndHeapAllocFailsThenCreateCallFails, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> mockCreateFile(&NEO::SysCalls::sysCallsCreateFile, [](LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) -> HANDLE {
        return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x7));
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> mockDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        if (static_cast<uint32_t>(dwIoControlCode) == PmtSysman::IoctlPmtGetTelemetryDiscoverySize) {
            *static_cast<unsigned long *>(lpOutBuffer) = 40;
        }
        return true;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> mockCmGetDeviceInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, [](PULONG pulLen, LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, ULONG ulFlags) -> CONFIGRET {
        *pulLen = 4;
        return CR_SUCCESS;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> mockCmGetDeviceInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, [](LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, PZZWSTR buffer, ULONG bufferLen, ULONG ulFlags) -> CONFIGRET {
        return CR_SUCCESS;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsHeapAlloc)> mockHeapAlloc(&NEO::SysCalls::sysCallsHeapAlloc, [](HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes) -> LPVOID {
        return nullptr;
    });

    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper());
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateAndTelemetryIndexIsGreaterThanTotalPmtInterfacesThenCreateCallFails, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> mockCreateFile(&NEO::SysCalls::sysCallsCreateFile, [](LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) -> HANDLE {
        return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x7));
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> mockDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        if (static_cast<uint32_t>(dwIoControlCode) == PmtSysman::IoctlPmtGetTelemetryDiscoverySize) {
            *static_cast<unsigned long *>(lpOutBuffer) = 40;
        } else if (static_cast<uint32_t>(dwIoControlCode) == PmtSysman::IoctlPmtGetTelemetryDiscovery) {
            // Initialize PmtTelemetryDiscovery 'temp' with {version = 1, interface count = 1, {{version = 1, index = 3, guid = 0x5e2f8210, dWordCount = 10}}}
            PmtSysman::PmtTelemetryDiscovery temp = {1, 1, {{1, 3, 0x5e2f8210, 10}}};
            *static_cast<PmtSysman::PmtTelemetryDiscovery *>(lpOutBuffer) = temp;
        }
        return true;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> mockCmGetDeviceInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, [](PULONG pulLen, LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, ULONG ulFlags) -> CONFIGRET {
        *pulLen = 4;
        return CR_SUCCESS;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> mockCmGetDeviceInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, [](LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, PZZWSTR buffer, ULONG bufferLen, ULONG ulFlags) -> CONFIGRET {
        return CR_SUCCESS;
    });
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper());
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateAndHeapFreeCallFailsThenCreateCallFails, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> mockCreateFile(&NEO::SysCalls::sysCallsCreateFile, [](LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) -> HANDLE {
        return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x7));
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> mockDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        if (static_cast<uint32_t>(dwIoControlCode) == PmtSysman::IoctlPmtGetTelemetryDiscoverySize) {
            *static_cast<unsigned long *>(lpOutBuffer) = 40;
        } else if (static_cast<uint32_t>(dwIoControlCode) == PmtSysman::IoctlPmtGetTelemetryDiscovery) {
            PmtSysman::PmtTelemetryDiscovery temp = {1, 2, {{1, 1, 0x5e2f8210, 10}}};
            *static_cast<PmtSysman::PmtTelemetryDiscovery *>(lpOutBuffer) = temp;
        }
        return true;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> mockCmGetDeviceInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, [](PULONG pulLen, LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, ULONG ulFlags) -> CONFIGRET {
        *pulLen = 4;
        return CR_SUCCESS;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> mockCmGetDeviceInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, [](LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, PZZWSTR buffer, ULONG bufferLen, ULONG ulFlags) -> CONFIGRET {
        return CR_SUCCESS;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsHeapFree)> mocksHeapFree(&NEO::SysCalls::sysCallsHeapFree, [](HANDLE hHeap, DWORD dwFlags, LPVOID lpMem) -> BOOL {
        return false;
    });
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper());
    EXPECT_EQ(nullptr, pPmt);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
