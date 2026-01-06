/*
 * Copyright (C) 2024-2025 Intel Corporation
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

static uint32_t sysCallsCmGetChildCount = 1;
static uint32_t sysCallsCmGetSiblingCount = 1;
static uint32_t createCallCount = 2;
static int sysCallsInterfaceListCounter = 0;

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

inline static BOOL mockDeviceIoControlExtendedSuccess(HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) {
    if (static_cast<uint32_t>(dwIoControlCode) == PmtSysman::IoctlPmtGetTelemetryDiscoverySize) {
        *static_cast<unsigned long *>(lpOutBuffer) = 40;
    } else if (static_cast<uint32_t>(dwIoControlCode) == PmtSysman::IoctlPmtGetTelemetryDiscovery) {
        PmtSysman::PmtTelemetryDiscovery temp = {1, 2, {{1, 1, 0x5e2f8210, 10}}};
        *static_cast<PmtSysman::PmtTelemetryDiscovery *>(lpOutBuffer) = temp;
    }
    return true;
}

inline static CONFIGRET mockCmGetDeviceInterfaceListSizeSuccess(PULONG pulLen, LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, ULONG ulFlags) {
    if (pDeviceID[0] != L'\0') {
        *pulLen = 40;
        return CR_SUCCESS;
    }
    return CR_FAILURE;
}

inline static CONFIGRET mockCmGetDeviceInterfaceListSuccess(LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, PZZWSTR buffer, ULONG bufferLen, ULONG ulFlags) {
    wcscpy(buffer, L"TEST_INTC_PMT");
    return CR_SUCCESS;
}

inline static CONFIGRET mockCmGetDeviceIdSizeSuccess(PULONG pulLen, DEVINST dnDevInst, ULONG ulFlags) {
    *pulLen = 40;
    return CR_SUCCESS;
}

inline static CONFIGRET mockCmGetDeviceIdSuccess(DEVINST dnDevInst, PWSTR buffer, ULONG bufferLen, ULONG ulFlags) {
    wcscpy(buffer, L"TEST_INTC_PMT");
    return CR_SUCCESS;
}

inline static BOOL mockSetupDiGetDeviceRegistryPropertySuccess(HDEVINFO deviceInfoSet, PSP_DEVINFO_DATA deviceInfoData, DWORD property, PDWORD propertyRegDataType, PBYTE propertyBuffer, DWORD propertyBufferSize, PDWORD requiredSize) {
    if (requiredSize != nullptr) {
        *requiredSize = 0x1;
        return false;
    }
    *propertyBuffer = 0u;
    return true;
}

inline static CONFIGRET mockCmGetChildSuccess(PDEVINST pdnDevInst, DEVINST dnDevInst, ULONG ulFlags) {
    if (sysCallsCmGetChildCount) {
        sysCallsCmGetChildCount--;
        return CR_SUCCESS;
    }
    return CR_FAILURE;
}

inline static CONFIGRET mockCmGetSiblingSuccess(PDEVINST pdnDevInst, DEVINST dnDevInst, ULONG ulFlags) {
    if (sysCallsCmGetSiblingCount) {
        sysCallsCmGetSiblingCount--;
        return CR_SUCCESS;
    }
    return CR_FAILURE;
}

inline static BOOL mockSetupDiEnumDeviceInfoSuccess(HDEVINFO deviceInfoSet, DWORD memberIndex, PSP_DEVINFO_DATA deviceInfoData) {
    return true;
}

inline static BOOL mockSetupDiOpenDeviceInfoSuccess(HDEVINFO deviceInfoSet, PCWSTR deviceInstanceId, HWND hwndParent, DWORD openFlags, PSP_DEVINFO_DATA deviceInfoData) {
    return true;
}

inline static BOOL mockSetupDiDestroyDeviceInfoListSuccess(HDEVINFO deviceInfoSet) {
    return true;
}

inline static HDEVINFO mockSetupDiGetClassDevsSuccess(GUID *classGuid, PCWSTR enumerator, HWND hwndParent, DWORD flags) {
    return reinterpret_cast<HDEVINFO>(static_cast<uintptr_t>(0x1));
}

class SysmanDevicePmtFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<PublicPlatformMonitoringTech> pPmt;
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        pPmt = std::make_unique<PublicPlatformMonitoringTech>(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
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
    pPmt = std::make_unique<PublicPlatformMonitoringTech>(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    pPmt->deviceInterface = L0::Sysman::ult::deviceInterface;
    uint32_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pPmt->readValue("DUMMY_KEY", val));
}

TEST_F(SysmanDevicePmtFixture, GivenInvalidPmtHandleWhenCallingReadValue64CallFails) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, [](LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) -> HANDLE {
        return INVALID_HANDLE_VALUE;
    });
    pPmt = std::make_unique<PublicPlatformMonitoringTech>(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    pPmt->deviceInterface = L0::Sysman::ult::deviceInterface;
    uint64_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pPmt->readValue("DUMMY_KEY", val));
}

TEST_F(SysmanDevicePmtFixture, GivenValidPmtHandleWhenCallingReadValueWithUint32WhenIoctlCallFailsThenProperErrorIsReturned) {
    pPmt = std::make_unique<PublicPlatformMonitoringTech>(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    pPmt->deviceInterface = L0::Sysman::ult::deviceInterface;
    pPmt->pcreateFile = mockCreateFileSuccess;
    uint32_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pPmt->readValue("DUMMY_KEY", val));
}

TEST_F(SysmanDevicePmtFixture, GivenValidPmtHandleWhenCallingReadValueWithUint32TypeWhenIoctlCallSucceedsThenReadValueReturned) {
    pPmt = std::make_unique<PublicPlatformMonitoringTech>(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    pPmt->deviceInterface = L0::Sysman::ult::deviceInterface;
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
    pPmt = std::make_unique<PublicPlatformMonitoringTech>(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    pPmt->deviceInterface = L0::Sysman::ult::deviceInterface;
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
    pPmt = std::make_unique<PublicPlatformMonitoringTech>(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    pPmt->deviceInterface = L0::Sysman::ult::deviceInterface;
    pPmt->pcreateFile = mockCreateFileSuccess;

    uint64_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pPmt->readValue("DUMMY_KEY", val));
}

TEST_F(SysmanDevicePmtFixture, GivenValidPmtHandleWhenCallingReadValueWithUint64TypeAndIoctlCallSucceedsThenreadValueReturned) {
    pPmt = std::make_unique<PublicPlatformMonitoringTech>(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    pPmt->deviceInterface = L0::Sysman::ult::deviceInterface;
    pPmt->pcreateFile = mockCreateFileSuccess;
    pPmt->pdeviceIoControl = mockDeviceIoControlSuccess;
    uint64_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pPmt->readValue("DUMMY_KEY", val));
    EXPECT_NE(0u, val);
}

TEST_F(SysmanDevicePmtFixture, GivenInvalidPmtInterfaceWhenCallingCreateThenCallReturnsNullPtr) {
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    EXPECT_EQ(nullptr, pPmt);
}

TEST_F(SysmanDevicePmtFixture, GivenInvalidPmtInterfaceWhenCallingGetSysmanPmtThenCallReturnsNullPtr) {
    PlatformMonitoringTech *pPmt = pWddmSysmanImp->getSysmanPmt();
    EXPECT_EQ(nullptr, pPmt);
}

TEST_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWithUnsupportedGuidWhenCallingCreateThenCallReturnsNullPtr) {
    sysCallsCmGetChildCount = 1;
    sysCallsCmGetSiblingCount = 1;
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, &mockCreateFileSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, &mockDeviceIoControlExtendedSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, &mockCmGetDeviceInterfaceListSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, &mockCmGetDeviceInterfaceListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceIdSize)> psysCallsCmGetDeviceIdSize(&NEO::SysCalls::sysCallsCmGetDeviceIdSize, &mockCmGetDeviceIdSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceId)> psysCallsCmGetDeviceId(&NEO::SysCalls::sysCallsCmGetDeviceId, &mockCmGetDeviceIdSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetChild)> psysCallsCmGetChild(&NEO::SysCalls::sysCallsCmGetChild, &mockCmGetChildSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetSibling)> psysCallsCmGetSibling(&NEO::SysCalls::sysCallsCmGetSibling, &mockCmGetSiblingSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty)> psysCallsSetupDiGetDeviceRegistryProperty(&NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty, &mockSetupDiGetDeviceRegistryPropertySuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo)> psysCallsSetupDiOpenDeviceInfo(&NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo, &mockSetupDiOpenDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo)> psysCallsSetupDiEnumDeviceInfo(&NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo, &mockSetupDiEnumDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList)> psysCallsSetupDiDestroyDeviceInfoList(&NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList, &mockSetupDiDestroyDeviceInfoListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetClassDevs)> psysCallsSetupDiGetClassDevs(&NEO::SysCalls::sysCallsSetupDiGetClassDevs, &mockSetupDiGetClassDevsSuccess);

    struct MockSysmanProductHelperPmt : L0::Sysman::SysmanProductHelperHw<IGFX_UNKNOWN> {
        MockSysmanProductHelperPmt() = default;
    };

    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper = std::make_unique<MockSysmanProductHelperPmt>();
    std::swap(pWddmSysmanImp->pSysmanProductHelper, pSysmanProductHelper);
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateThenCallSucceeds, IsBMG) {
    sysCallsCmGetChildCount = 1;
    sysCallsCmGetSiblingCount = 1;
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, &mockCreateFileSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, &mockDeviceIoControlExtendedSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, &mockCmGetDeviceInterfaceListSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, &mockCmGetDeviceInterfaceListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceIdSize)> psysCallsCmGetDeviceIdSize(&NEO::SysCalls::sysCallsCmGetDeviceIdSize, &mockCmGetDeviceIdSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceId)> psysCallsCmGetDeviceId(&NEO::SysCalls::sysCallsCmGetDeviceId, &mockCmGetDeviceIdSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetChild)> psysCallsCmGetChild(&NEO::SysCalls::sysCallsCmGetChild, &mockCmGetChildSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetSibling)> psysCallsCmGetSibling(&NEO::SysCalls::sysCallsCmGetSibling, &mockCmGetSiblingSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty)> psysCallsSetupDiGetDeviceRegistryProperty(&NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty, &mockSetupDiGetDeviceRegistryPropertySuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo)> psysCallsSetupDiOpenDeviceInfo(&NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo, &mockSetupDiOpenDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo)> psysCallsSetupDiEnumDeviceInfo(&NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo, &mockSetupDiEnumDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList)> psysCallsSetupDiDestroyDeviceInfoList(&NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList, &mockSetupDiDestroyDeviceInfoListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetClassDevs)> psysCallsSetupDiGetClassDevs(&NEO::SysCalls::sysCallsSetupDiGetClassDevs, &mockSetupDiGetClassDevsSuccess);
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    EXPECT_NE(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenInValidPmtInterfaceNameWhenCallingCreateThenCallFails, IsBMG) {
    sysCallsCmGetChildCount = 1;
    sysCallsCmGetSiblingCount = 1;
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, &mockCreateFileSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, &mockDeviceIoControlExtendedSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, &mockCmGetDeviceInterfaceListSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, [](LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, PZZWSTR buffer, ULONG bufferLen, ULONG ulFlags) -> CONFIGRET {
        wcscpy(buffer, L"TEST_WRONG_INTERFACE");
        return CR_SUCCESS;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceIdSize)> psysCallsCmGetDeviceIdSize(&NEO::SysCalls::sysCallsCmGetDeviceIdSize, &mockCmGetDeviceIdSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceId)> psysCallsCmGetDeviceId(&NEO::SysCalls::sysCallsCmGetDeviceId, &mockCmGetDeviceIdSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetChild)> psysCallsCmGetChild(&NEO::SysCalls::sysCallsCmGetChild, &mockCmGetChildSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetSibling)> psysCallsCmGetSibling(&NEO::SysCalls::sysCallsCmGetSibling, &mockCmGetSiblingSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty)> psysCallsSetupDiGetDeviceRegistryProperty(&NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty, &mockSetupDiGetDeviceRegistryPropertySuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo)> psysCallsSetupDiOpenDeviceInfo(&NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo, &mockSetupDiOpenDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo)> psysCallsSetupDiEnumDeviceInfo(&NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo, &mockSetupDiEnumDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList)> psysCallsSetupDiDestroyDeviceInfoList(&NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList, &mockSetupDiDestroyDeviceInfoListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetClassDevs)> psysCallsSetupDiGetClassDevs(&NEO::SysCalls::sysCallsSetupDiGetClassDevs, &mockSetupDiGetClassDevsSuccess);
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateWithNoGuidsFoundThenCallFails, IsBMG) {
    sysCallsCmGetChildCount = 1;
    sysCallsCmGetSiblingCount = 1;
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, &mockCreateFileSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        if (static_cast<uint32_t>(dwIoControlCode) == PmtSysman::IoctlPmtGetTelemetryDiscoverySize) {
            *static_cast<unsigned long *>(lpOutBuffer) = 40;
        }
        return true;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, &mockCmGetDeviceInterfaceListSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, &mockCmGetDeviceInterfaceListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceIdSize)> psysCallsCmGetDeviceIdSize(&NEO::SysCalls::sysCallsCmGetDeviceIdSize, &mockCmGetDeviceIdSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceId)> psysCallsCmGetDeviceId(&NEO::SysCalls::sysCallsCmGetDeviceId, &mockCmGetDeviceIdSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetChild)> psysCallsCmGetChild(&NEO::SysCalls::sysCallsCmGetChild, &mockCmGetChildSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetSibling)> psysCallsCmGetSibling(&NEO::SysCalls::sysCallsCmGetSibling, &mockCmGetSiblingSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty)> psysCallsSetupDiGetDeviceRegistryProperty(&NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty, &mockSetupDiGetDeviceRegistryPropertySuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo)> psysCallsSetupDiOpenDeviceInfo(&NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo, &mockSetupDiOpenDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo)> psysCallsSetupDiEnumDeviceInfo(&NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo, &mockSetupDiEnumDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList)> psysCallsSetupDiDestroyDeviceInfoList(&NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList, &mockSetupDiDestroyDeviceInfoListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetClassDevs)> psysCallsSetupDiGetClassDevs(&NEO::SysCalls::sysCallsSetupDiGetClassDevs, &mockSetupDiGetClassDevsSuccess);
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateAndTelemtrySizeIsZeroThenCallFails, IsBMG) {
    sysCallsCmGetChildCount = 1;
    sysCallsCmGetSiblingCount = 1;
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, &mockCreateFileSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        if (static_cast<uint32_t>(dwIoControlCode) == PmtSysman::IoctlPmtGetTelemetryDiscoverySize) {
            *static_cast<unsigned long *>(lpOutBuffer) = 0;
        }
        return true;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, &mockCmGetDeviceInterfaceListSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, &mockCmGetDeviceInterfaceListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceIdSize)> psysCallsCmGetDeviceIdSize(&NEO::SysCalls::sysCallsCmGetDeviceIdSize, &mockCmGetDeviceIdSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceId)> psysCallsCmGetDeviceId(&NEO::SysCalls::sysCallsCmGetDeviceId, &mockCmGetDeviceIdSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetChild)> psysCallsCmGetChild(&NEO::SysCalls::sysCallsCmGetChild, &mockCmGetChildSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetSibling)> psysCallsCmGetSibling(&NEO::SysCalls::sysCallsCmGetSibling, &mockCmGetSiblingSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty)> psysCallsSetupDiGetDeviceRegistryProperty(&NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty, &mockSetupDiGetDeviceRegistryPropertySuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo)> psysCallsSetupDiOpenDeviceInfo(&NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo, &mockSetupDiOpenDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo)> psysCallsSetupDiEnumDeviceInfo(&NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo, &mockSetupDiEnumDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList)> psysCallsSetupDiDestroyDeviceInfoList(&NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList, &mockSetupDiDestroyDeviceInfoListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetClassDevs)> psysCallsSetupDiGetClassDevs(&NEO::SysCalls::sysCallsSetupDiGetClassDevs, &mockSetupDiGetClassDevsSuccess);
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateAndTelemtryDiscoveryFailsThenCallFails, IsBMG) {
    sysCallsCmGetChildCount = 1;
    sysCallsCmGetSiblingCount = 1;
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, &mockCreateFileSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        if (static_cast<uint32_t>(dwIoControlCode) == PmtSysman::IoctlPmtGetTelemetryDiscoverySize) {
            *static_cast<unsigned long *>(lpOutBuffer) = 40;
        } else if (static_cast<uint32_t>(dwIoControlCode) == PmtSysman::IoctlPmtGetTelemetryDiscovery) {
            return false;
        }
        return true;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, &mockCmGetDeviceInterfaceListSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, &mockCmGetDeviceInterfaceListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceIdSize)> psysCallsCmGetDeviceIdSize(&NEO::SysCalls::sysCallsCmGetDeviceIdSize, &mockCmGetDeviceIdSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceId)> psysCallsCmGetDeviceId(&NEO::SysCalls::sysCallsCmGetDeviceId, &mockCmGetDeviceIdSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetChild)> psysCallsCmGetChild(&NEO::SysCalls::sysCallsCmGetChild, &mockCmGetChildSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetSibling)> psysCallsCmGetSibling(&NEO::SysCalls::sysCallsCmGetSibling, &mockCmGetSiblingSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty)> psysCallsSetupDiGetDeviceRegistryProperty(&NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty, &mockSetupDiGetDeviceRegistryPropertySuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo)> psysCallsSetupDiOpenDeviceInfo(&NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo, &mockSetupDiOpenDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo)> psysCallsSetupDiEnumDeviceInfo(&NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo, &mockSetupDiEnumDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList)> psysCallsSetupDiDestroyDeviceInfoList(&NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList, &mockSetupDiDestroyDeviceInfoListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetClassDevs)> psysCallsSetupDiGetClassDevs(&NEO::SysCalls::sysCallsSetupDiGetClassDevs, &mockSetupDiGetClassDevsSuccess);
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateAndTelemtryDiscoveryFailsThenCallFailsCoveringNewBranch, IsBMG) {
    sysCallsCmGetChildCount = 1;
    sysCallsCmGetSiblingCount = 1;
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, &mockCreateFileSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, &mockCmGetDeviceInterfaceListSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, &mockCmGetDeviceInterfaceListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceIdSize)> psysCallsCmGetDeviceIdSize(&NEO::SysCalls::sysCallsCmGetDeviceIdSize, &mockCmGetDeviceIdSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceId)> psysCallsCmGetDeviceId(&NEO::SysCalls::sysCallsCmGetDeviceId, &mockCmGetDeviceIdSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetChild)> psysCallsCmGetChild(&NEO::SysCalls::sysCallsCmGetChild, &mockCmGetChildSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetSibling)> psysCallsCmGetSibling(&NEO::SysCalls::sysCallsCmGetSibling, &mockCmGetSiblingSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty)> psysCallsSetupDiGetDeviceRegistryProperty(&NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty, &mockSetupDiGetDeviceRegistryPropertySuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo)> psysCallsSetupDiOpenDeviceInfo(&NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo, &mockSetupDiOpenDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo)> psysCallsSetupDiEnumDeviceInfo(&NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo, &mockSetupDiEnumDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList)> psysCallsSetupDiDestroyDeviceInfoList(&NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList, &mockSetupDiDestroyDeviceInfoListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetClassDevs)> psysCallsSetupDiGetClassDevs(&NEO::SysCalls::sysCallsSetupDiGetClassDevs, &mockSetupDiGetClassDevsSuccess);
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateThenCallSucceedsCoveringNewBranch, IsBMG) {
    sysCallsCmGetChildCount = 1;
    sysCallsCmGetSiblingCount = 1;
    sysCallsInterfaceListCounter = 0;
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, &mockCreateFileSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, &mockDeviceIoControlExtendedSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, &mockCmGetDeviceInterfaceListSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, [](LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, PZZWSTR buffer, ULONG bufferLen, ULONG ulFlags) -> CONFIGRET {
        if (sysCallsInterfaceListCounter) {
            wcscpy(buffer, L"TEST_INTC_PMT");
            return CR_SUCCESS;
        }
        sysCallsInterfaceListCounter++;
        return CR_BUFFER_SMALL;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceIdSize)> psysCallsCmGetDeviceIdSize(&NEO::SysCalls::sysCallsCmGetDeviceIdSize, &mockCmGetDeviceIdSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceId)> psysCallsCmGetDeviceId(&NEO::SysCalls::sysCallsCmGetDeviceId, &mockCmGetDeviceIdSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetChild)> psysCallsCmGetChild(&NEO::SysCalls::sysCallsCmGetChild, &mockCmGetChildSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetSibling)> psysCallsCmGetSibling(&NEO::SysCalls::sysCallsCmGetSibling, &mockCmGetSiblingSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty)> psysCallsSetupDiGetDeviceRegistryProperty(&NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty, &mockSetupDiGetDeviceRegistryPropertySuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo)> psysCallsSetupDiOpenDeviceInfo(&NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo, &mockSetupDiOpenDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo)> psysCallsSetupDiEnumDeviceInfo(&NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo, &mockSetupDiEnumDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList)> psysCallsSetupDiDestroyDeviceInfoList(&NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList, &mockSetupDiDestroyDeviceInfoListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetClassDevs)> psysCallsSetupDiGetClassDevs(&NEO::SysCalls::sysCallsSetupDiGetClassDevs, &mockSetupDiGetClassDevsSuccess);
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    EXPECT_NE(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenInValidPmtInterfaceWhenCallingCreateThenCallFails, IsBMG) {
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateAndCmGetDeviceInterfaceListFailsThenCallFails, IsBMG) {
    sysCallsCmGetChildCount = 1;
    sysCallsCmGetSiblingCount = 1;
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, [](PULONG pulLen, LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, ULONG ulFlags) -> CONFIGRET {
        if (pDeviceID[0] != L'\0') {
            *pulLen = 4;
            return CR_SUCCESS;
        }
        return CR_FAILURE;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceIdSize)> psysCallsCmGetDeviceIdSize(&NEO::SysCalls::sysCallsCmGetDeviceIdSize, &mockCmGetDeviceIdSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceId)> psysCallsCmGetDeviceId(&NEO::SysCalls::sysCallsCmGetDeviceId, &mockCmGetDeviceIdSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetChild)> psysCallsCmGetChild(&NEO::SysCalls::sysCallsCmGetChild, &mockCmGetChildSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetSibling)> psysCallsCmGetSibling(&NEO::SysCalls::sysCallsCmGetSibling, &mockCmGetSiblingSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty)> psysCallsSetupDiGetDeviceRegistryProperty(&NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty, &mockSetupDiGetDeviceRegistryPropertySuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo)> psysCallsSetupDiOpenDeviceInfo(&NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo, &mockSetupDiOpenDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo)> psysCallsSetupDiEnumDeviceInfo(&NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo, &mockSetupDiEnumDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList)> psysCallsSetupDiDestroyDeviceInfoList(&NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList, &mockSetupDiDestroyDeviceInfoListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetClassDevs)> psysCallsSetupDiGetClassDevs(&NEO::SysCalls::sysCallsSetupDiGetClassDevs, &mockSetupDiGetClassDevsSuccess);
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateAndCmGetDeviceInterfaceListSizeReturnZeroThenCallFails, IsBMG) {
    sysCallsCmGetChildCount = 1;
    sysCallsCmGetSiblingCount = 1;
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, [](PULONG pulLen, LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, ULONG ulFlags) -> CONFIGRET {
        if (pDeviceID[0] != L'\0') {
            *pulLen = 0;
            return CR_SUCCESS;
        }
        return CR_FAILURE;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, [](LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, PZZWSTR buffer, ULONG bufferLen, ULONG ulFlags) -> CONFIGRET {
        return CR_SUCCESS;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceIdSize)> psysCallsCmGetDeviceIdSize(&NEO::SysCalls::sysCallsCmGetDeviceIdSize, &mockCmGetDeviceIdSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceId)> psysCallsCmGetDeviceId(&NEO::SysCalls::sysCallsCmGetDeviceId, &mockCmGetDeviceIdSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetChild)> psysCallsCmGetChild(&NEO::SysCalls::sysCallsCmGetChild, &mockCmGetChildSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetSibling)> psysCallsCmGetSibling(&NEO::SysCalls::sysCallsCmGetSibling, &mockCmGetSiblingSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty)> psysCallsSetupDiGetDeviceRegistryProperty(&NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty, &mockSetupDiGetDeviceRegistryPropertySuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo)> psysCallsSetupDiOpenDeviceInfo(&NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo, &mockSetupDiOpenDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo)> psysCallsSetupDiEnumDeviceInfo(&NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo, &mockSetupDiEnumDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList)> psysCallsSetupDiDestroyDeviceInfoList(&NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList, &mockSetupDiDestroyDeviceInfoListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetClassDevs)> psysCallsSetupDiGetClassDevs(&NEO::SysCalls::sysCallsSetupDiGetClassDevs, &mockSetupDiGetClassDevsSuccess);
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateAndHeapAllocFailsThenCreateCallFails, IsBMG) {
    sysCallsCmGetChildCount = 1;
    sysCallsCmGetSiblingCount = 1;
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, &mockCreateFileSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> mockDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        if (static_cast<uint32_t>(dwIoControlCode) == PmtSysman::IoctlPmtGetTelemetryDiscoverySize) {
            *static_cast<unsigned long *>(lpOutBuffer) = 40;
        }
        return true;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, &mockCmGetDeviceInterfaceListSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, &mockCmGetDeviceInterfaceListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsHeapAlloc)> mockHeapAlloc(&NEO::SysCalls::sysCallsHeapAlloc, [](HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes) -> LPVOID {
        return nullptr;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceIdSize)> psysCallsCmGetDeviceIdSize(&NEO::SysCalls::sysCallsCmGetDeviceIdSize, &mockCmGetDeviceIdSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceId)> psysCallsCmGetDeviceId(&NEO::SysCalls::sysCallsCmGetDeviceId, &mockCmGetDeviceIdSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetChild)> psysCallsCmGetChild(&NEO::SysCalls::sysCallsCmGetChild, &mockCmGetChildSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetSibling)> psysCallsCmGetSibling(&NEO::SysCalls::sysCallsCmGetSibling, &mockCmGetSiblingSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty)> psysCallsSetupDiGetDeviceRegistryProperty(&NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty, &mockSetupDiGetDeviceRegistryPropertySuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo)> psysCallsSetupDiOpenDeviceInfo(&NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo, &mockSetupDiOpenDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo)> psysCallsSetupDiEnumDeviceInfo(&NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo, &mockSetupDiEnumDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList)> psysCallsSetupDiDestroyDeviceInfoList(&NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList, &mockSetupDiDestroyDeviceInfoListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetClassDevs)> psysCallsSetupDiGetClassDevs(&NEO::SysCalls::sysCallsSetupDiGetClassDevs, &mockSetupDiGetClassDevsSuccess);
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateAndTelemetryIndexIsGreaterThanTotalPmtInterfacesThenCreateCallFails, IsBMG) {
    sysCallsCmGetChildCount = 1;
    sysCallsCmGetSiblingCount = 1;
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, &mockCreateFileSuccess);
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
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, &mockCmGetDeviceInterfaceListSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, &mockCmGetDeviceInterfaceListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceIdSize)> psysCallsCmGetDeviceIdSize(&NEO::SysCalls::sysCallsCmGetDeviceIdSize, &mockCmGetDeviceIdSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceId)> psysCallsCmGetDeviceId(&NEO::SysCalls::sysCallsCmGetDeviceId, &mockCmGetDeviceIdSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetChild)> psysCallsCmGetChild(&NEO::SysCalls::sysCallsCmGetChild, &mockCmGetChildSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetSibling)> psysCallsCmGetSibling(&NEO::SysCalls::sysCallsCmGetSibling, &mockCmGetSiblingSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty)> psysCallsSetupDiGetDeviceRegistryProperty(&NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty, &mockSetupDiGetDeviceRegistryPropertySuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo)> psysCallsSetupDiOpenDeviceInfo(&NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo, &mockSetupDiOpenDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo)> psysCallsSetupDiEnumDeviceInfo(&NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo, &mockSetupDiEnumDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList)> psysCallsSetupDiDestroyDeviceInfoList(&NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList, &mockSetupDiDestroyDeviceInfoListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetClassDevs)> psysCallsSetupDiGetClassDevs(&NEO::SysCalls::sysCallsSetupDiGetClassDevs, &mockSetupDiGetClassDevsSuccess);
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateAndHeapFreeCallFailsThenCreateCallFails, IsBMG) {
    sysCallsCmGetChildCount = 1;
    sysCallsCmGetSiblingCount = 1;
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, &mockCreateFileSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, &mockDeviceIoControlExtendedSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, &mockCmGetDeviceInterfaceListSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, &mockCmGetDeviceInterfaceListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsHeapFree)> mocksHeapFree(&NEO::SysCalls::sysCallsHeapFree, [](HANDLE hHeap, DWORD dwFlags, LPVOID lpMem) -> BOOL {
        return false;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceIdSize)> psysCallsCmGetDeviceIdSize(&NEO::SysCalls::sysCallsCmGetDeviceIdSize, &mockCmGetDeviceIdSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceId)> psysCallsCmGetDeviceId(&NEO::SysCalls::sysCallsCmGetDeviceId, &mockCmGetDeviceIdSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetChild)> psysCallsCmGetChild(&NEO::SysCalls::sysCallsCmGetChild, &mockCmGetChildSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetSibling)> psysCallsCmGetSibling(&NEO::SysCalls::sysCallsCmGetSibling, &mockCmGetSiblingSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty)> psysCallsSetupDiGetDeviceRegistryProperty(&NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty, &mockSetupDiGetDeviceRegistryPropertySuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo)> psysCallsSetupDiOpenDeviceInfo(&NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo, &mockSetupDiOpenDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo)> psysCallsSetupDiEnumDeviceInfo(&NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo, &mockSetupDiEnumDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList)> psysCallsSetupDiDestroyDeviceInfoList(&NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList, &mockSetupDiDestroyDeviceInfoListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetClassDevs)> psysCallsSetupDiGetClassDevs(&NEO::SysCalls::sysCallsSetupDiGetClassDevs, &mockSetupDiGetClassDevsSuccess);
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateAndSetupDiGetClassDevsCallFailsThenNullptrIsReturned, IsBMG) {
    sysCallsCmGetChildCount = 1;
    sysCallsCmGetSiblingCount = 1;
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, &mockCreateFileSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, &mockDeviceIoControlExtendedSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, &mockCmGetDeviceInterfaceListSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, &mockCmGetDeviceInterfaceListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceIdSize)> psysCallsCmGetDeviceIdSize(&NEO::SysCalls::sysCallsCmGetDeviceIdSize, &mockCmGetDeviceIdSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceId)> psysCallsCmGetDeviceId(&NEO::SysCalls::sysCallsCmGetDeviceId, &mockCmGetDeviceIdSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetChild)> psysCallsCmGetChild(&NEO::SysCalls::sysCallsCmGetChild, &mockCmGetChildSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetSibling)> psysCallsCmGetSibling(&NEO::SysCalls::sysCallsCmGetSibling, &mockCmGetSiblingSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty)> psysCallsSetupDiGetDeviceRegistryProperty(&NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty, &mockSetupDiGetDeviceRegistryPropertySuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo)> psysCallsSetupDiOpenDeviceInfo(&NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo, &mockSetupDiOpenDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo)> psysCallsSetupDiEnumDeviceInfo(&NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo, &mockSetupDiEnumDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList)> psysCallsSetupDiDestroyDeviceInfoList(&NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList, &mockSetupDiDestroyDeviceInfoListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetClassDevs)> psysCallsSetupDiGetClassDevs(&NEO::SysCalls::sysCallsSetupDiGetClassDevs, [](GUID *classGuid, PCWSTR enumerator, HWND hwndParent, DWORD flags) -> HDEVINFO {
        return reinterpret_cast<HDEVINFO>(INVALID_HANDLE_VALUE);
    });
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateAndSetupDiDestroyDeviceInfoListFailsThenNullptrIsReturned, IsBMG) {
    sysCallsCmGetChildCount = 1;
    sysCallsCmGetSiblingCount = 1;
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, &mockCreateFileSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, &mockDeviceIoControlExtendedSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, &mockCmGetDeviceInterfaceListSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, &mockCmGetDeviceInterfaceListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceIdSize)> psysCallsCmGetDeviceIdSize(&NEO::SysCalls::sysCallsCmGetDeviceIdSize, &mockCmGetDeviceIdSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceId)> psysCallsCmGetDeviceId(&NEO::SysCalls::sysCallsCmGetDeviceId, &mockCmGetDeviceIdSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetChild)> psysCallsCmGetChild(&NEO::SysCalls::sysCallsCmGetChild, &mockCmGetChildSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetSibling)> psysCallsCmGetSibling(&NEO::SysCalls::sysCallsCmGetSibling, &mockCmGetSiblingSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty)> psysCallsSetupDiGetDeviceRegistryProperty(&NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty, &mockSetupDiGetDeviceRegistryPropertySuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo)> psysCallsSetupDiOpenDeviceInfo(&NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo, &mockSetupDiOpenDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo)> psysCallsSetupDiEnumDeviceInfo(&NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo, [](HDEVINFO deviceInfoSet, DWORD memberIndex, PSP_DEVINFO_DATA deviceInfoData) -> BOOL {
        return false;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList)> psysCallsSetupDiDestroyDeviceInfoList(&NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList, [](HDEVINFO deviceInfoSet) -> BOOL {
        return false;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetClassDevs)> psysCallsSetupDiGetClassDevs(&NEO::SysCalls::sysCallsSetupDiGetClassDevs, &mockSetupDiGetClassDevsSuccess);
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateAndSetupDiEnumDeviceInfoFailsThenNullptrIsReturned, IsBMG) {
    sysCallsCmGetChildCount = 1;
    sysCallsCmGetSiblingCount = 1;
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, &mockCreateFileSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, &mockDeviceIoControlExtendedSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, &mockCmGetDeviceInterfaceListSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, &mockCmGetDeviceInterfaceListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceIdSize)> psysCallsCmGetDeviceIdSize(&NEO::SysCalls::sysCallsCmGetDeviceIdSize, &mockCmGetDeviceIdSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceId)> psysCallsCmGetDeviceId(&NEO::SysCalls::sysCallsCmGetDeviceId, &mockCmGetDeviceIdSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetChild)> psysCallsCmGetChild(&NEO::SysCalls::sysCallsCmGetChild, &mockCmGetChildSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetSibling)> psysCallsCmGetSibling(&NEO::SysCalls::sysCallsCmGetSibling, &mockCmGetSiblingSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty)> psysCallsSetupDiGetDeviceRegistryProperty(&NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty, &mockSetupDiGetDeviceRegistryPropertySuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo)> psysCallsSetupDiOpenDeviceInfo(&NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo, &mockSetupDiOpenDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo)> psysCallsSetupDiEnumDeviceInfo(&NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo, [](HDEVINFO deviceInfoSet, DWORD memberIndex, PSP_DEVINFO_DATA deviceInfoData) -> BOOL {
        return false;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList)> psysCallsSetupDiDestroyDeviceInfoList(&NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList, &mockSetupDiDestroyDeviceInfoListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetClassDevs)> psysCallsSetupDiGetClassDevs(&NEO::SysCalls::sysCallsSetupDiGetClassDevs, &mockSetupDiGetClassDevsSuccess);
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateAndGetDeviceRegistryPropertyFailsThenNullptrIsReturned, IsBMG) {
    sysCallsCmGetChildCount = 1;
    sysCallsCmGetSiblingCount = 1;
    createCallCount = 2;
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, &mockCreateFileSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, &mockDeviceIoControlExtendedSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, &mockCmGetDeviceInterfaceListSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, &mockCmGetDeviceInterfaceListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceIdSize)> psysCallsCmGetDeviceIdSize(&NEO::SysCalls::sysCallsCmGetDeviceIdSize, &mockCmGetDeviceIdSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceId)> psysCallsCmGetDeviceId(&NEO::SysCalls::sysCallsCmGetDeviceId, &mockCmGetDeviceIdSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetChild)> psysCallsCmGetChild(&NEO::SysCalls::sysCallsCmGetChild, &mockCmGetChildSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetSibling)> psysCallsCmGetSibling(&NEO::SysCalls::sysCallsCmGetSibling, &mockCmGetSiblingSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty)> psysCallsSetupDiGetDeviceRegistryProperty(&NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty, [](HDEVINFO deviceInfoSet, PSP_DEVINFO_DATA deviceInfoData, DWORD property, PDWORD propertyRegDataType, PBYTE propertyBuffer, DWORD propertyBufferSize, PDWORD requiredSize) -> BOOL {
        if (createCallCount == 1) {
            *requiredSize = 0x0;
            return true;
        } else {
            if (requiredSize != nullptr) {
                *requiredSize = 0x1;
                return false;
            }
            return false;
        }
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo)> psysCallsSetupDiOpenDeviceInfo(&NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo, &mockSetupDiOpenDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo)> psysCallsSetupDiEnumDeviceInfo(&NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo, [](HDEVINFO deviceInfoSet, DWORD memberIndex, PSP_DEVINFO_DATA deviceInfoData) -> BOOL {
        if (createCallCount) {
            createCallCount--;
            return true;
        }
        return false;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList)> psysCallsSetupDiDestroyDeviceInfoList(&NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList, &mockSetupDiDestroyDeviceInfoListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetClassDevs)> psysCallsSetupDiGetClassDevs(&NEO::SysCalls::sysCallsSetupDiGetClassDevs, &mockSetupDiGetClassDevsSuccess);
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateAndGetDeviceRegistryPropertyFailsThenNullptrIsReturnedCoveringAdditionalBranch, IsBMG) {
    sysCallsCmGetChildCount = 1;
    sysCallsCmGetSiblingCount = 1;
    createCallCount = 1;
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, &mockCreateFileSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, &mockDeviceIoControlExtendedSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, &mockCmGetDeviceInterfaceListSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, &mockCmGetDeviceInterfaceListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceIdSize)> psysCallsCmGetDeviceIdSize(&NEO::SysCalls::sysCallsCmGetDeviceIdSize, &mockCmGetDeviceIdSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceId)> psysCallsCmGetDeviceId(&NEO::SysCalls::sysCallsCmGetDeviceId, &mockCmGetDeviceIdSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetChild)> psysCallsCmGetChild(&NEO::SysCalls::sysCallsCmGetChild, &mockCmGetChildSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetSibling)> psysCallsCmGetSibling(&NEO::SysCalls::sysCallsCmGetSibling, &mockCmGetSiblingSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty)> psysCallsSetupDiGetDeviceRegistryProperty(&NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty, [](HDEVINFO deviceInfoSet, PSP_DEVINFO_DATA deviceInfoData, DWORD property, PDWORD propertyRegDataType, PBYTE propertyBuffer, DWORD propertyBufferSize, PDWORD requiredSize) -> BOOL {
        if (property == SPDRP_BUSNUMBER) {
            if (requiredSize != nullptr) {
                *requiredSize = 0x1;
                return false;
            }
            *propertyBuffer = 0u;
            return true;
        } else {
            return true;
        }
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo)> psysCallsSetupDiOpenDeviceInfo(&NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo, &mockSetupDiOpenDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo)> psysCallsSetupDiEnumDeviceInfo(&NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo, [](HDEVINFO deviceInfoSet, DWORD memberIndex, PSP_DEVINFO_DATA deviceInfoData) -> BOOL {
        if (createCallCount) {
            createCallCount--;
            return true;
        }
        return false;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList)> psysCallsSetupDiDestroyDeviceInfoList(&NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList, &mockSetupDiDestroyDeviceInfoListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetClassDevs)> psysCallsSetupDiGetClassDevs(&NEO::SysCalls::sysCallsSetupDiGetClassDevs, &mockSetupDiGetClassDevsSuccess);
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateAndPciBDFMatchingFailsThenNullptrIsReturned, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, &mockCreateFileSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, &mockDeviceIoControlExtendedSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, &mockCmGetDeviceInterfaceListSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, &mockCmGetDeviceInterfaceListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceIdSize)> psysCallsCmGetDeviceIdSize(&NEO::SysCalls::sysCallsCmGetDeviceIdSize, &mockCmGetDeviceIdSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceId)> psysCallsCmGetDeviceId(&NEO::SysCalls::sysCallsCmGetDeviceId, &mockCmGetDeviceIdSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetChild)> psysCallsCmGetChild(&NEO::SysCalls::sysCallsCmGetChild, &mockCmGetChildSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetSibling)> psysCallsCmGetSibling(&NEO::SysCalls::sysCallsCmGetSibling, &mockCmGetSiblingSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty)> psysCallsSetupDiGetDeviceRegistryProperty(&NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty, [](HDEVINFO deviceInfoSet, PSP_DEVINFO_DATA deviceInfoData, DWORD property, PDWORD propertyRegDataType, PBYTE propertyBuffer, DWORD propertyBufferSize, PDWORD requiredSize) -> BOOL {
        if (requiredSize != nullptr) {
            *requiredSize = 0x1;
            return false;
        }
        *propertyBuffer = 0u;
        return true;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo)> psysCallsSetupDiOpenDeviceInfo(&NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo, &mockSetupDiOpenDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo)> psysCallsSetupDiEnumDeviceInfo(&NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo, [](HDEVINFO deviceInfoSet, DWORD memberIndex, PSP_DEVINFO_DATA deviceInfoData) -> BOOL {
        if (createCallCount) {
            createCallCount--;
            return true;
        }
        return false;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList)> psysCallsSetupDiDestroyDeviceInfoList(&NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList, &mockSetupDiDestroyDeviceInfoListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetClassDevs)> psysCallsSetupDiGetClassDevs(&NEO::SysCalls::sysCallsSetupDiGetClassDevs, &mockSetupDiGetClassDevsSuccess);
    std::vector<std::vector<uint32_t>> devicePciBdfCombinations = {{1, 1, 1}, {1, 1, 0}, {1, 0, 1}, {1, 0, 0}, {0, 1, 1}, {0, 1, 0}, {0, 0, 1}};
    for (uint32_t i = 0; i < devicePciBdfCombinations.size(); i++) {
        sysCallsCmGetChildCount = 1;
        sysCallsCmGetSiblingCount = 1;
        createCallCount = 1;

        std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper(), devicePciBdfCombinations[i][0], devicePciBdfCombinations[i][1], devicePciBdfCombinations[i][2]);
        EXPECT_EQ(nullptr, pPmt);
    }
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateAndCmGetDeviceIdSizeFailsThenNullptrIsReturned, IsBMG) {
    sysCallsCmGetChildCount = 1;
    sysCallsCmGetSiblingCount = 1;
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, &mockCreateFileSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, &mockDeviceIoControlExtendedSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, &mockCmGetDeviceInterfaceListSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, &mockCmGetDeviceInterfaceListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceIdSize)> psysCallsCmGetDeviceIdSize(&NEO::SysCalls::sysCallsCmGetDeviceIdSize, [](PULONG pulLen, DEVINST dnDevInst, ULONG ulFlags) -> CONFIGRET {
        return CR_FAILURE;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceId)> psysCallsCmGetDeviceId(&NEO::SysCalls::sysCallsCmGetDeviceId, &mockCmGetDeviceIdSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetChild)> psysCallsCmGetChild(&NEO::SysCalls::sysCallsCmGetChild, &mockCmGetChildSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetSibling)> psysCallsCmGetSibling(&NEO::SysCalls::sysCallsCmGetSibling, &mockCmGetSiblingSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty)> psysCallsSetupDiGetDeviceRegistryProperty(&NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty, &mockSetupDiGetDeviceRegistryPropertySuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo)> psysCallsSetupDiOpenDeviceInfo(&NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo, &mockSetupDiOpenDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo)> psysCallsSetupDiEnumDeviceInfo(&NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo, &mockSetupDiEnumDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList)> psysCallsSetupDiDestroyDeviceInfoList(&NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList, &mockSetupDiDestroyDeviceInfoListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetClassDevs)> psysCallsSetupDiGetClassDevs(&NEO::SysCalls::sysCallsSetupDiGetClassDevs, &mockSetupDiGetClassDevsSuccess);
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateAndCmGetDeviceIdFailsThenNullptrIsReturned, IsBMG) {
    sysCallsCmGetChildCount = 1;
    sysCallsCmGetSiblingCount = 1;
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, &mockCreateFileSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, &mockDeviceIoControlExtendedSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, &mockCmGetDeviceInterfaceListSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, &mockCmGetDeviceInterfaceListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceIdSize)> psysCallsCmGetDeviceIdSize(&NEO::SysCalls::sysCallsCmGetDeviceIdSize, &mockCmGetDeviceIdSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceId)> psysCallsCmGetDeviceId(&NEO::SysCalls::sysCallsCmGetDeviceId, [](DEVINST dnDevInst, PWSTR buffer, ULONG bufferLen, ULONG ulFlags) -> CONFIGRET {
        return CR_FAILURE;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetChild)> psysCallsCmGetChild(&NEO::SysCalls::sysCallsCmGetChild, &mockCmGetChildSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetSibling)> psysCallsCmGetSibling(&NEO::SysCalls::sysCallsCmGetSibling, &mockCmGetSiblingSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty)> psysCallsSetupDiGetDeviceRegistryProperty(&NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty, &mockSetupDiGetDeviceRegistryPropertySuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo)> psysCallsSetupDiOpenDeviceInfo(&NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo, &mockSetupDiOpenDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo)> psysCallsSetupDiEnumDeviceInfo(&NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo, &mockSetupDiEnumDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList)> psysCallsSetupDiDestroyDeviceInfoList(&NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList, &mockSetupDiDestroyDeviceInfoListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetClassDevs)> psysCallsSetupDiGetClassDevs(&NEO::SysCalls::sysCallsSetupDiGetClassDevs, &mockSetupDiGetClassDevsSuccess);
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateAndCmGetDeviceIdReturnEmptyIdThenNullptrIsReturned, IsBMG) {
    sysCallsCmGetChildCount = 1;
    sysCallsCmGetSiblingCount = 1;
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, &mockCreateFileSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, &mockDeviceIoControlExtendedSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, &mockCmGetDeviceInterfaceListSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, &mockCmGetDeviceInterfaceListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceIdSize)> psysCallsCmGetDeviceIdSize(&NEO::SysCalls::sysCallsCmGetDeviceIdSize, [](PULONG pulLen, DEVINST dnDevInst, ULONG ulFlags) -> CONFIGRET {
        return CR_SUCCESS;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceId)> psysCallsCmGetDeviceId(&NEO::SysCalls::sysCallsCmGetDeviceId, [](DEVINST dnDevInst, PWSTR buffer, ULONG bufferLen, ULONG ulFlags) -> CONFIGRET {
        return CR_SUCCESS;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetChild)> psysCallsCmGetChild(&NEO::SysCalls::sysCallsCmGetChild, &mockCmGetChildSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetSibling)> psysCallsCmGetSibling(&NEO::SysCalls::sysCallsCmGetSibling, &mockCmGetSiblingSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty)> psysCallsSetupDiGetDeviceRegistryProperty(&NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty, &mockSetupDiGetDeviceRegistryPropertySuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo)> psysCallsSetupDiOpenDeviceInfo(&NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo, &mockSetupDiOpenDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo)> psysCallsSetupDiEnumDeviceInfo(&NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo, &mockSetupDiEnumDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList)> psysCallsSetupDiDestroyDeviceInfoList(&NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList, &mockSetupDiDestroyDeviceInfoListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetClassDevs)> psysCallsSetupDiGetClassDevs(&NEO::SysCalls::sysCallsSetupDiGetClassDevs, &mockSetupDiGetClassDevsSuccess);
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateAndSetupDiOpenDeviceInfoFailsThenNullptrIsReturned, IsBMG) {
    sysCallsCmGetChildCount = 1;
    sysCallsCmGetSiblingCount = 1;
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, &mockCreateFileSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, &mockDeviceIoControlExtendedSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, &mockCmGetDeviceInterfaceListSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, &mockCmGetDeviceInterfaceListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceIdSize)> psysCallsCmGetDeviceIdSize(&NEO::SysCalls::sysCallsCmGetDeviceIdSize, &mockCmGetDeviceIdSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceId)> psysCallsCmGetDeviceId(&NEO::SysCalls::sysCallsCmGetDeviceId, &mockCmGetDeviceIdSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetChild)> psysCallsCmGetChild(&NEO::SysCalls::sysCallsCmGetChild, &mockCmGetChildSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetSibling)> psysCallsCmGetSibling(&NEO::SysCalls::sysCallsCmGetSibling, &mockCmGetSiblingSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty)> psysCallsSetupDiGetDeviceRegistryProperty(&NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty, &mockSetupDiGetDeviceRegistryPropertySuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo)> psysCallsSetupDiOpenDeviceInfo(&NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo, [](HDEVINFO deviceInfoSet, PCWSTR deviceInstanceId, HWND hwndParent, DWORD openFlags, PSP_DEVINFO_DATA deviceInfoData) -> BOOL {
        return false;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo)> psysCallsSetupDiEnumDeviceInfo(&NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo, &mockSetupDiEnumDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList)> psysCallsSetupDiDestroyDeviceInfoList(&NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList, &mockSetupDiDestroyDeviceInfoListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetClassDevs)> psysCallsSetupDiGetClassDevs(&NEO::SysCalls::sysCallsSetupDiGetClassDevs, &mockSetupDiGetClassDevsSuccess);
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    EXPECT_EQ(nullptr, pPmt);
}

HWTEST2_F(SysmanDevicePmtFixture, GivenValidPmtInterfaceWhenCallingCreateAndCmGetDeviceIdReturnsWrongIdThenNullptrIsReturned, IsBMG) {
    sysCallsCmGetChildCount = 1;
    sysCallsCmGetSiblingCount = 1;
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, &mockCreateFileSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, &mockDeviceIoControlExtendedSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize)> psysCallsInterfaceListSize(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceListSize, &mockCmGetDeviceInterfaceListSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceInterfaceList)> psysCallsInterfaceList(&NEO::SysCalls::sysCallsCmGetDeviceInterfaceList, &mockCmGetDeviceInterfaceListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceIdSize)> psysCallsCmGetDeviceIdSize(&NEO::SysCalls::sysCallsCmGetDeviceIdSize, &mockCmGetDeviceIdSizeSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetDeviceId)> psysCallsCmGetDeviceId(&NEO::SysCalls::sysCallsCmGetDeviceId, [](DEVINST dnDevInst, PWSTR buffer, ULONG bufferLen, ULONG ulFlags) -> CONFIGRET {
        wcscpy(buffer, L"INVALID_TEST_ID");
        return CR_SUCCESS;
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetChild)> psysCallsCmGetChild(&NEO::SysCalls::sysCallsCmGetChild, &mockCmGetChildSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsCmGetSibling)> psysCallsCmGetSibling(&NEO::SysCalls::sysCallsCmGetSibling, &mockCmGetSiblingSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty)> psysCallsSetupDiGetDeviceRegistryProperty(&NEO::SysCalls::sysCallsSetupDiGetDeviceRegistryProperty, &mockSetupDiGetDeviceRegistryPropertySuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo)> psysCallsSetupDiOpenDeviceInfo(&NEO::SysCalls::sysCallsSetupDiOpenDeviceInfo, &mockSetupDiOpenDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo)> psysCallsSetupDiEnumDeviceInfo(&NEO::SysCalls::sysCallsSetupDiEnumDeviceInfo, &mockSetupDiEnumDeviceInfoSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList)> psysCallsSetupDiDestroyDeviceInfoList(&NEO::SysCalls::sysCallsSetupDiDestroyDeviceInfoList, &mockSetupDiDestroyDeviceInfoListSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsSetupDiGetClassDevs)> psysCallsSetupDiGetClassDevs(&NEO::SysCalls::sysCallsSetupDiGetClassDevs, &mockSetupDiGetClassDevsSuccess);
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create(pWddmSysmanImp->getSysmanProductHelper(), 0, 0, 0);
    EXPECT_EQ(nullptr, pPmt);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
