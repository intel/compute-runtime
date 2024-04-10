/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"

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
    *(int *)lpOutBuffer = 1;
    return true;
}

class SysmanDevicePmtFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<PublicPlatformMonitoringTech> pPmt;
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        std::vector<wchar_t> deviceInterface;
        pPmt = std::make_unique<PublicPlatformMonitoringTech>(deviceInterface);
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
    pPmt = std::make_unique<PublicPlatformMonitoringTech>(deviceInterface);
    uint32_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pPmt->readValue("DUMMY_KEY", val));
}

TEST_F(SysmanDevicePmtFixture, GivenInvalidPmtHandleWhenCallingReadValue64CallFails) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, [](LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) -> HANDLE {
        return INVALID_HANDLE_VALUE;
    });
    pPmt = std::make_unique<PublicPlatformMonitoringTech>(deviceInterface);
    uint64_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pPmt->readValue("DUMMY_KEY", val));
}

TEST_F(SysmanDevicePmtFixture, GivenValidPmtHandleWhenCallingReadValueWithUint32WhenIoctlCallFailsThenProperErrorIsReturned) {
    pPmt = std::make_unique<PublicPlatformMonitoringTech>(deviceInterface);
    pPmt->pcreateFile = mockCreateFileSuccess;
    uint32_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pPmt->readValue("DUMMY_KEY", val));
}

TEST_F(SysmanDevicePmtFixture, GivenValidPmtHandleWhenCallingReadValueWithUint32TypeWhenIoctlCallSucceedsThenreadValueReturned) {
    pPmt = std::make_unique<PublicPlatformMonitoringTech>(deviceInterface);
    pPmt->pcreateFile = mockCreateFileSuccess;
    pPmt->pdeviceIoControl = mockDeviceIoControlSuccess;

    uint32_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pPmt->readValue("DUMMY_KEY", val));
    EXPECT_NE(0u, val);
}

TEST_F(SysmanDevicePmtFixture, GivenValidPmtHandleWhenCallingReadValueWithUint32WithNullValueReturnedFromIoctlCallThenreadValueFails) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        *lpBytesReturned = 4;
        *(int *)lpOutBuffer = 0;
        return true;
    });
    pPmt = std::make_unique<PublicPlatformMonitoringTech>(deviceInterface);
    pPmt->pcreateFile = mockCreateFileSuccess;

    uint32_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pPmt->readValue("DUMMY_KEY", val));
}

TEST_F(SysmanDevicePmtFixture, GivenValidPmtHandleWhenCallingReadValueWithUint32WithNullBytesCountReturnedFromIoctlCallThenreadValueFails) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        *lpBytesReturned = 0;
        *(int *)lpOutBuffer = 4;
        return true;
    });
    pPmt = std::make_unique<PublicPlatformMonitoringTech>(deviceInterface);
    pPmt->pcreateFile = mockCreateFileSuccess;

    uint32_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pPmt->readValue("DUMMY_KEY", val));
}

TEST_F(SysmanDevicePmtFixture, GivenValidPmtHandleWhenCallingReadValueWithUint64WithNullValueReturnedFromIoctlCallThenreadValueFails) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        *lpBytesReturned = 4;
        *(int *)lpOutBuffer = 0;
        return true;
    });
    pPmt = std::make_unique<PublicPlatformMonitoringTech>(deviceInterface);
    pPmt->pcreateFile = mockCreateFileSuccess;

    uint64_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pPmt->readValue("DUMMY_KEY", val));
}

TEST_F(SysmanDevicePmtFixture, GivenValidPmtHandleWhenCallingReadValueWithUint64WithNullBytesCountReturnedFromIoctlCallThenreadValueFails) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        *lpBytesReturned = 0;
        *(int *)lpOutBuffer = 4;
        return true;
    });
    pPmt = std::make_unique<PublicPlatformMonitoringTech>(deviceInterface);
    pPmt->pcreateFile = mockCreateFileSuccess;

    uint64_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, pPmt->readValue("DUMMY_KEY", val));
}

TEST_F(SysmanDevicePmtFixture, GivenValidPmtHandleWhenCallingreadValueWithUint64TypeAndIoctlCallSucceedsThenreadValueReturned) {
    pPmt = std::make_unique<PublicPlatformMonitoringTech>(deviceInterface);
    pPmt->pcreateFile = mockCreateFileSuccess;
    pPmt->pdeviceIoControl = mockDeviceIoControlSuccess;
    uint64_t val = 0;
    pPmt->keyOffsetMap = dummyKeyOffsetMap;
    EXPECT_EQ(ZE_RESULT_SUCCESS, pPmt->readValue("DUMMY_KEY", val));
    EXPECT_NE(0u, val);
}

TEST_F(SysmanDevicePmtFixture, GivenInvalidPmtInterfaceWhenCallingCreateThenCallReturnsNullPtr) {
    std::unique_ptr<PlatformMonitoringTech> pPmt = PublicPlatformMonitoringTech::create();
    EXPECT_EQ(nullptr, pPmt);
}

TEST_F(SysmanDevicePmtFixture, GivenInvalidPmtInterfaceWhenCallingGetSysmanPmtThenCallReturnsNullPtr) {
    PlatformMonitoringTech *pPmt = pWddmSysmanImp->getSysmanPmt();
    EXPECT_EQ(nullptr, pPmt);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
