/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/windows/mock_sys_calls.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/sysman/source/api/memory/windows/sysman_os_memory_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/memory/windows/mock_memory.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

const std::map<std::string, std::pair<uint32_t, uint32_t>> dummyKeyOffsetMap = {
    {{"MSU_BITMASK", {922, 1}},
     {"VRAM_BANDWIDTH", {14, 0}},
     {"GDDR0_CH0_GT_32B_RD_REQ_UPPER", {94, 1}},
     {"GDDR0_CH0_GT_32B_RD_REQ_LOWER", {95, 1}},
     {"GDDR0_CH1_GT_32B_RD_REQ_UPPER", {114, 1}},
     {"GDDR0_CH1_GT_32B_RD_REQ_LOWER", {115, 1}},
     {"GDDR0_CH0_DISPLAYVC0_32B_RD_REQ_UPPER", {102, 1}},
     {"GDDR0_CH0_DISPLAYVC0_32B_RD_REQ_LOWER", {103, 1}},
     {"GDDR0_CH1_DISPLAYVC0_32B_RD_REQ_UPPER", {122, 1}},
     {"GDDR0_CH1_DISPLAYVC0_32B_RD_REQ_LOWER", {123, 1}},
     {"GDDR0_CH0_SOC_32B_RD_REQ_UPPER", {106, 1}},
     {"GDDR0_CH0_SOC_32B_RD_REQ_LOWER", {107, 1}},
     {"GDDR0_CH1_SOC_32B_RD_REQ_UPPER", {126, 1}},
     {"GDDR0_CH1_SOC_32B_RD_REQ_LOWER", {127, 1}},
     {"GDDR0_CH0_SOC_32B_WR_REQ_UPPER", {110, 1}},
     {"GDDR0_CH0_SOC_32B_WR_REQ_LOWER", {111, 1}},
     {"GDDR0_CH1_SOC_32B_WR_REQ_UPPER", {130, 1}},
     {"GDDR0_CH1_SOC_32B_WR_REQ_LOWER", {131, 1}},
     {"GDDR_TELEM_CAPTURE_TIMESTAMP_UPPER", {93, 1}},
     {"GDDR_TELEM_CAPTURE_TIMESTAMP_LOWER", {92, 1}},
     {"GDDR0_CH0_GT_64B_RD_REQ_UPPER", {96, 1}},
     {"GDDR0_CH0_GT_64B_RD_REQ_LOWER", {97, 1}},
     {"GDDR0_CH1_GT_64B_RD_REQ_UPPER", {116, 1}},
     {"GDDR0_CH1_GT_64B_RD_REQ_LOWER", {117, 1}},
     {"GDDR0_CH0_DISPLAYVC0_64B_RD_REQ_UPPER", {104, 1}},
     {"GDDR0_CH0_DISPLAYVC0_64B_RD_REQ_LOWER", {105, 1}},
     {"GDDR0_CH1_DISPLAYVC0_64B_RD_REQ_UPPER", {124, 1}},
     {"GDDR0_CH1_DISPLAYVC0_64B_RD_REQ_LOWER", {125, 1}},
     {"GDDR0_CH0_SOC_64B_RD_REQ_UPPER", {108, 1}},
     {"GDDR0_CH0_SOC_64B_RD_REQ_LOWER", {109, 1}},
     {"GDDR0_CH1_SOC_64B_RD_REQ_UPPER", {128, 1}},
     {"GDDR0_CH1_SOC_64B_RD_REQ_LOWER", {129, 1}},
     {"GDDR0_CH0_SOC_64B_WR_REQ_UPPER", {112, 1}},
     {"GDDR0_CH0_SOC_64B_WR_REQ_LOWER", {113, 1}},
     {"GDDR0_CH1_SOC_64B_WR_REQ_UPPER", {132, 1}},
     {"GDDR0_CH1_SOC_64B_WR_REQ_LOWER", {133, 1}},
     {"GDDR0_CH0_GT_32B_WR_REQ_UPPER", {98, 1}},
     {"GDDR0_CH0_GT_32B_WR_REQ_LOWER", {99, 1}},
     {"GDDR0_CH1_GT_32B_WR_REQ_UPPER", {118, 1}},
     {"GDDR0_CH1_GT_32B_WR_REQ_LOWER", {119, 1}},
     {"GDDR0_CH0_GT_64B_WR_REQ_UPPER", {100, 1}},
     {"GDDR0_CH0_GT_64B_WR_REQ_LOWER", {101, 1}},
     {"GDDR0_CH1_GT_64B_WR_REQ_UPPER", {120, 1}},
     {"GDDR0_CH1_GT_64B_WR_REQ_LOWER", {121, 1}}}};

const std::wstring pmtInterfaceName = L"TEST\0";
std::vector<wchar_t> deviceInterfaceMemory(pmtInterfaceName.begin(), pmtInterfaceName.end());
class SysmanDeviceMemoryHelperFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<MockMemoryKmdSysManager> pKmdSysManager;
    L0::Sysman::KmdSysManager *pOriginalKmdSysManager = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();

        pKmdSysManager.reset(new MockMemoryKmdSysManager);

        pKmdSysManager->allowSetCalls = true;

        pOriginalKmdSysManager = pWddmSysmanImp->pKmdSysManager;
        pWddmSysmanImp->pKmdSysManager = pKmdSysManager.get();

        auto pPmt = new PublicPlatformMonitoringTech(deviceInterfaceMemory, pWddmSysmanImp->getSysmanProductHelper());
        pPmt->keyOffsetMap = dummyKeyOffsetMap;
        pWddmSysmanImp->pPmt.reset(pPmt);

        pSysmanDeviceImp->pMemoryHandleContext->handleList.clear();
        getMemoryHandles(0);
    }

    void TearDown() override {
        pWddmSysmanImp->pKmdSysManager = pOriginalKmdSysManager;
        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_mem_handle_t> getMemoryHandles(uint32_t count) {
        std::vector<zes_mem_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumMemoryModules(pSysmanDevice->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }

    MockMemoryManagerSysman *pMemoryManager = nullptr;
};

HWTEST2_F(SysmanDeviceMemoryHelperFixture, GivenValidMemoryHandleWhenGettingBandwidthThenCallSucceeds, IsBMG) {
    static constexpr uint32_t mockPmtBandWidthVariableBackupValue = 100;
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, [](LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) -> HANDLE {
        return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x7));
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        PmtSysman::PmtTelemetryRead *readRequest = static_cast<PmtSysman::PmtTelemetryRead *>(lpInBuffer);
        switch (readRequest->offset) {
        case 922:
            *lpBytesReturned = 8;
            *static_cast<uint32_t *>(lpOutBuffer) = 1;
            return true;
        case 14:
            *lpBytesReturned = 8;
            *static_cast<uint32_t *>(lpOutBuffer) = 131072;
            return true;
        default:
            *lpBytesReturned = 8;
            if (readRequest->offset % 2 == 0) {
                *static_cast<uint32_t *>(lpOutBuffer) = 0;
            } else {
                *static_cast<uint32_t *>(lpOutBuffer) = mockPmtBandWidthVariableBackupValue;
            }
            return true;
        }
    });
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_bandwidth_t bandwidth;

        ze_result_t result = zesMemoryGetBandwidth(handle, &bandwidth);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(bandwidth.maxBandwidth, static_cast<uint64_t>(mockMemoryMaxBandwidth * megaBytesToBytes * 100));
        EXPECT_EQ(bandwidth.readCounter, (6 * mockPmtBandWidthVariableBackupValue * 32) + (6 * mockPmtBandWidthVariableBackupValue * 64));
        EXPECT_EQ(bandwidth.writeCounter, (4 * mockPmtBandWidthVariableBackupValue * 32) + (4 * mockPmtBandWidthVariableBackupValue * 64));
        EXPECT_GT(bandwidth.timestamp, 0u);
    }
}

HWTEST2_F(SysmanDeviceMemoryHelperFixture, GivenValidMemoryHandleWhenGettingBandwidthCoveringNegativePathsThenCallFails, IsBMG) {
    static uint32_t count = 6;
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, [](LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) -> HANDLE {
        return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x7));
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        PmtSysman::PmtTelemetryRead *readRequest = static_cast<PmtSysman::PmtTelemetryRead *>(lpInBuffer);
        switch (readRequest->offset) {
        case 922:
            *lpBytesReturned = 8;
            *static_cast<uint32_t *>(lpOutBuffer) = 1;
            return count == 1 ? false : true;
        case 14:
            *lpBytesReturned = 8;
            *static_cast<uint32_t *>(lpOutBuffer) = 131072;
            return count == 2 ? false : true;
        case 94:
            *lpBytesReturned = 8;
            *static_cast<uint32_t *>(lpOutBuffer) = 0;
            return count == 3 ? false : true;
        case 95:
            *lpBytesReturned = 8;
            *static_cast<uint32_t *>(lpOutBuffer) = 10000000;
            return count == 4 ? false : true;
        case 110:
            *lpBytesReturned = 8;
            *static_cast<uint32_t *>(lpOutBuffer) = 0;
            return count == 5 ? false : true;
        case 111:
            *lpBytesReturned = 8;
            *static_cast<uint32_t *>(lpOutBuffer) = 10000000;
            return count == 6 ? false : true;
        default:
            *lpBytesReturned = 8;
            if (readRequest->offset % 2 == 0) {
                *static_cast<uint32_t *>(lpOutBuffer) = 0;
            } else {
                *static_cast<uint32_t *>(lpOutBuffer) = 10000000;
            }
            return true;
        }
    });
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_bandwidth_t bandwidth;

        while (count) {
            ze_result_t result = zesMemoryGetBandwidth(handle, &bandwidth);
            ASSERT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
            count--;
        }
    }
}

HWTEST2_F(SysmanDeviceMemoryHelperFixture, GivenNullPmtHandleWhenGettingBandwidthThenCallFails, IsBMG) {
    pWddmSysmanImp->pPmt.reset(nullptr);
    auto handles = getMemoryHandles(memoryHandleComponentCount);
    for (auto handle : handles) {
        zes_mem_bandwidth_t bandwidth;
        ASSERT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesMemoryGetBandwidth(handle, &bandwidth));
    }
}

HWTEST2_F(SysmanDeviceMemoryHelperFixture, GivenValidMemoryHandleWhenGettingBandwidthThenCallSucceeds, IsAtMostDg2) {
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_bandwidth_t bandwidth;

        ze_result_t result = zesMemoryGetBandwidth(handle, &bandwidth);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(bandwidth.maxBandwidth, static_cast<uint64_t>(pKmdSysManager->mockMemoryMaxBandwidth) * mbpsToBytesPerSecond);
        EXPECT_EQ(bandwidth.readCounter, pKmdSysManager->mockMemoryCurrentBandwidthRead);
        EXPECT_EQ(bandwidth.writeCounter, pKmdSysManager->mockMemoryCurrentBandwidthWrite);
        EXPECT_GT(bandwidth.timestamp, 0u);

        std::vector<uint32_t> requestId = {KmdSysman::Requests::Memory::MaxBandwidth, KmdSysman::Requests::Memory::CurrentBandwidthRead, KmdSysman::Requests::Memory::CurrentBandwidthWrite};
        for (auto it = requestId.begin(); it != requestId.end(); it++) {
            pKmdSysManager->mockMemoryFailure[*it] = 1;
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesMemoryGetBandwidth(handle, &bandwidth));
            pKmdSysManager->mockMemoryFailure[*it] = 0;
        }
    }
}

HWTEST2_F(SysmanDeviceMemoryHelperFixture, GivenValidMemoryHandleWhenGettingBandwidthCoverningNegativePathsThenCallFails, IsAtMostDg2) {
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_bandwidth_t bandwidth;

        pKmdSysManager->mockRequestMultiple = true;
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesMemoryGetBandwidth(handle, &bandwidth));
        pKmdSysManager->mockRequestMultiple = false;
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
