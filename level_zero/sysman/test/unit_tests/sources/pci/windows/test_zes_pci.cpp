/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/windows/mock_sys_calls.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/sysman/source/sysman_const.h"
#include "level_zero/sysman/test/unit_tests/sources/pci/windows/mock_pci.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_fixture.h"

#include "mock_pci.h"

namespace L0 {
namespace Sysman {
namespace ult {

const std::map<std::string, std::pair<uint32_t, uint32_t>> dummyKeyOffsetMap = {
    {{"rx_byte_count_lsb", {70, 1}},
     {"rx_byte_count_msb", {69, 1}},
     {"tx_byte_count_lsb", {72, 1}},
     {"tx_byte_count_msb", {71, 1}},
     {"rx_pkt_count_lsb", {74, 1}},
     {"rx_pkt_count_msb", {73, 1}},
     {"tx_pkt_count_lsb", {76, 1}},
     {"tx_pkt_count_msb", {75, 1}},
     {"GDDR_TELEM_CAPTURE_TIMESTAMP_UPPER", {92, 1}},
     {"GDDR_TELEM_CAPTURE_TIMESTAMP_LOWER", {93, 1}}}};

const std::wstring pmtInterfaceName = L"TEST\0";
std::vector<wchar_t> deviceInterfacePci(pmtInterfaceName.begin(), pmtInterfaceName.end());

class SysmanDevicePciFixture : public SysmanDeviceFixture {
  protected:
    std::unique_ptr<PciKmdSysManager> pKmdSysManager = nullptr;
    KmdSysManager *pOriginalKmdSysManager = nullptr;
    void SetUp() override {

        SysmanDeviceFixture::SetUp();

        pKmdSysManager.reset(new PciKmdSysManager);

        pOriginalKmdSysManager = pWddmSysmanImp->pKmdSysManager;
        pWddmSysmanImp->pKmdSysManager = pKmdSysManager.get();

        auto pPmt = new PublicPlatformMonitoringTech(deviceInterfacePci, pWddmSysmanImp->getSysmanProductHelper());
        pPmt->keyOffsetMap = dummyKeyOffsetMap;
        pWddmSysmanImp->pPmt.reset(pPmt);

        delete pSysmanDeviceImp->pPci;
        pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = false;
        pSysmanDeviceImp->pPci = new PciImp(pOsSysman);

        if (pSysmanDeviceImp->pPci) {
            pSysmanDeviceImp->pPci->init();
        }
    }

    void TearDown() override {
        pWddmSysmanImp->pKmdSysManager = pOriginalKmdSysManager;
        SysmanDeviceFixture::TearDown();
    }

    void setLocalMemorySupportedAndReinit(bool supported) {

        delete pSysmanDeviceImp->pPci;

        pSysmanDeviceImp->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = !supported;

        pSysmanDeviceImp->pPci = new PciImp(pOsSysman);

        if (pSysmanDeviceImp->pPci) {
            pSysmanDeviceImp->pPci->init();
        }
    }
};

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetPropertiesWithLocalMemoryThenVerifyzetSysmanPciGetPropertiesCallSucceeds) {

    setLocalMemorySupportedAndReinit(true);

    zes_pci_properties_t properties;

    ze_result_t result = zesDevicePciGetProperties(pSysmanDevice->toHandle(), &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(properties.address.domain, pKmdSysManager->mockDomain[KmdSysman::PciDomainsType::PciRootPort]);
    EXPECT_EQ(properties.address.bus, pKmdSysManager->mockBus[KmdSysman::PciDomainsType::PciRootPort]);
    EXPECT_EQ(properties.address.device, pKmdSysManager->mockDevice[KmdSysman::PciDomainsType::PciRootPort]);
    EXPECT_EQ(properties.address.function, pKmdSysManager->mockFunction[KmdSysman::PciDomainsType::PciRootPort]);
    EXPECT_EQ(static_cast<uint32_t>(properties.maxSpeed.gen), pKmdSysManager->mockMaxLinkSpeed[KmdSysman::PciDomainsType::PciRootPort]);
    EXPECT_EQ(static_cast<uint32_t>(properties.maxSpeed.width), pKmdSysManager->mockMaxLinkWidth[KmdSysman::PciDomainsType::PciRootPort]);
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallinggetPciBdfAndkmdSysmanCallFailsThenUnknownValuesArereturned) {
    setLocalMemorySupportedAndReinit(true);
    pKmdSysManager->pciBusReturnCode = KmdSysman::KmdSysmanFail;
    pKmdSysManager->pciDomainReturnCode = KmdSysman::KmdSysmanFail;
    pKmdSysManager->pciDeviceReturnCode = KmdSysman::KmdSysmanFail;
    pKmdSysManager->pciFunctionReturnCode = KmdSysman::KmdSysmanFail;
    zes_pci_properties_t properties = {};
    WddmPciImp *pPciImp = new WddmPciImp(pOsSysman);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pPciImp->getPciBdf(properties));
    EXPECT_EQ(0u, properties.address.domain);
    EXPECT_EQ(0u, properties.address.bus);
    EXPECT_EQ(0u, properties.address.device);
    EXPECT_EQ(0u, properties.address.function);
    delete pPciImp;
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetProLocalMemoryThenVerifyzetSysmanPciGetPropertiesCallSucceeds) {
    setLocalMemorySupportedAndReinit(true);

    zes_pci_properties_t properties;

    ze_result_t result = zesDevicePciGetProperties(pSysmanDevice->toHandle(), &properties);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(properties.address.domain, pKmdSysManager->mockDomain[KmdSysman::PciDomainsType::PciRootPort]);
    EXPECT_EQ(properties.address.bus, pKmdSysManager->mockBus[KmdSysman::PciDomainsType::PciRootPort]);
    EXPECT_EQ(properties.address.device, pKmdSysManager->mockDevice[KmdSysman::PciDomainsType::PciRootPort]);
    EXPECT_EQ(properties.address.function, pKmdSysManager->mockFunction[KmdSysman::PciDomainsType::PciRootPort]);
    EXPECT_EQ(static_cast<uint32_t>(properties.maxSpeed.gen), pKmdSysManager->mockMaxLinkSpeed[KmdSysman::PciDomainsType::PciRootPort]);
    EXPECT_EQ(static_cast<uint32_t>(properties.maxSpeed.width), pKmdSysManager->mockMaxLinkWidth[KmdSysman::PciDomainsType::PciRootPort]);
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallingGetPciBdfAndRequestMultpileFailsThenFailureIsReturned) {
    setLocalMemorySupportedAndReinit(true);
    pKmdSysManager->mockRequestMultiple = true;
    pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    zes_pci_properties_t properties = {};
    WddmPciImp *pPciImp = new WddmPciImp(pOsSysman);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pPciImp->getPciBdf(properties));
    delete pPciImp;
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenGettingMaxLinkSpeedAndMaxLinkWidthAndRequestSingleFailsThenUnknownValuesAreReturned) {
    setLocalMemorySupportedAndReinit(true);
    pKmdSysManager->mockRequestSingle = true;
    pKmdSysManager->mockRequestSingleResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    double maxLinkSpeed;
    int32_t maxLinkWidth;
    WddmPciImp *pPciImp = new WddmPciImp(pOsSysman);
    pPciImp->getMaxLinkCaps(maxLinkSpeed, maxLinkWidth);
    EXPECT_DOUBLE_EQ(0.0, maxLinkSpeed);
    EXPECT_EQ(-1, maxLinkWidth);
    delete pPciImp;
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetBarsThenVerifyzetSysmanPciGetBarsCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciGetBars(pSysmanDevice->toHandle(), &count, nullptr));
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetBarsThenVerifyzetSysmanPciGetBarsCallSucceedsWith1_2Extension) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciGetBars(pSysmanDevice->toHandle(), &count, nullptr));
    EXPECT_NE(count, 0u);

    std::vector<zes_pci_bar_properties_t> pBarProps(count);
    std::vector<zes_pci_bar_properties_1_2_t> props1dot2(count);
    for (uint32_t i = 0; i < count; i++) {
        props1dot2[i].stype = ZES_STRUCTURE_TYPE_PCI_BAR_PROPERTIES_1_2;
        props1dot2[i].pNext = nullptr;
        pBarProps[i].stype = ZES_STRUCTURE_TYPE_PCI_BAR_PROPERTIES;
        pBarProps[i].pNext = static_cast<void *>(&props1dot2[i]);
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciGetBars(pSysmanDevice->toHandle(), &count, pBarProps.data()));

    for (uint32_t i = 0; i < count; i++) {
        EXPECT_EQ(pBarProps[i].stype, zes_structure_type_t::ZES_STRUCTURE_TYPE_PCI_BAR_PROPERTIES);
        EXPECT_EQ(props1dot2[i].stype, zes_structure_type_t::ZES_STRUCTURE_TYPE_PCI_BAR_PROPERTIES_1_2);
        EXPECT_EQ(props1dot2[i].resizableBarSupported, true);
        EXPECT_EQ(props1dot2[i].resizableBarEnabled, true);
    }
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallingPciGetBarsThenVerifyAPICallSucceedsWith1_2ExtensionWithNullPtr) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciGetBars(pSysmanDevice->toHandle(), &count, nullptr));
    EXPECT_NE(count, 0u);

    zes_pci_bar_properties_t *pBarProps = new zes_pci_bar_properties_t[count];

    for (uint32_t i = 0; i < count; i++) {
        pBarProps[i].pNext = nullptr;
        pBarProps[i].stype = zes_structure_type_t::ZES_STRUCTURE_TYPE_PCI_BAR_PROPERTIES;
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciGetBars(pSysmanDevice->toHandle(), &count, pBarProps));

    delete[] pBarProps;
    pBarProps = nullptr;
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetBarsThenVerifyzetSysmanPciGetBarsCallSucceedsWith1_2ExtensionWrongType) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciGetBars(pSysmanDevice->toHandle(), &count, nullptr));
    EXPECT_NE(count, 0u);

    std::vector<zes_pci_bar_properties_t> pBarProps(count);
    std::vector<zes_pci_bar_properties_1_2_t> props1dot2(count);
    for (uint32_t i = 0; i < count; i++) {
        props1dot2[i].stype = ZES_STRUCTURE_TYPE_PCI_STATE;
        props1dot2[i].pNext = nullptr;
        pBarProps[i].stype = ZES_STRUCTURE_TYPE_PCI_BAR_PROPERTIES;
        pBarProps[i].pNext = static_cast<void *>(&props1dot2[i]);
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDevicePciGetBars(pSysmanDevice->toHandle(), &count, pBarProps.data()));

    for (uint32_t i = 0; i < count; i++) {
        EXPECT_EQ(pBarProps[i].stype, zes_structure_type_t::ZES_STRUCTURE_TYPE_PCI_BAR_PROPERTIES);
        EXPECT_LE(pBarProps[i].type, ZES_PCI_BAR_TYPE_MEM);
        EXPECT_EQ(props1dot2[i].stype, zes_structure_type_t::ZES_STRUCTURE_TYPE_PCI_STATE);
    }
}

HWTEST2_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallingzesPciGetStatsThenCallReturnsUnsupported, IsAtMostDg2) {
    zes_pci_stats_t stats;
    ze_result_t result = zesDevicePciGetStats(pSysmanDevice->toHandle(), &stats);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetStatsWithLocalMemoryThenVerifyzetSysmanPciGetBarsCallSucceeds) {
    setLocalMemorySupportedAndReinit(true);

    zes_pci_state_t state;
    ze_result_t result = zesDevicePciGetState(pSysmanDevice->toHandle(), &state);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<uint32_t>(state.speed.gen), pKmdSysManager->mockCurrentLinkSpeed[KmdSysman::PciDomainsType::PciRootPort]);
    EXPECT_EQ(static_cast<uint32_t>(state.speed.width), pKmdSysManager->mockCurrentLinkWidth[KmdSysman::PciDomainsType::PciRootPort]);
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetStatsWithNoLocalMemoryThenVerifyzetSysmanPciGetBarsCallSucceeds) {
    setLocalMemorySupportedAndReinit(false);

    zes_pci_state_t state;
    ze_result_t result = zesDevicePciGetState(pSysmanDevice->toHandle(), &state);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(static_cast<uint32_t>(state.speed.gen), pKmdSysManager->mockCurrentLinkSpeed[KmdSysman::PciDomainsType::PciCurrentDevice]);
    EXPECT_EQ(static_cast<uint32_t>(state.speed.width), pKmdSysManager->mockCurrentLinkWidth[KmdSysman::PciDomainsType::PciCurrentDevice]);
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallingzetSysmanPciGetStateThenValidCurrentMaxBandwidthIsReturned) {
    setLocalMemorySupportedAndReinit(true);

    zes_pci_state_t state = {};
    ze_result_t result = zesDevicePciGetState(pSysmanDevice->toHandle(), &state);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(state.speed.maxBandwidth, pKmdSysManager->mockCurrentMaxBandwidth[KmdSysman::PciDomainsType::PciRootPort]);
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallingGetPciStateAndRequestMultipleFailsThenFailureIsReturned) {
    setLocalMemorySupportedAndReinit(true);
    pKmdSysManager->mockRequestMultiple = true;
    pKmdSysManager->mockRequestMultipleResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    zes_pci_state_t pState = {};
    WddmPciImp *pPciImp = new WddmPciImp(pOsSysman);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pPciImp->getState(&pState));
    delete pPciImp;
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallingGetPciStateAndKmdSysmanCallFailsThenUnknownValuesAreReturned) {
    setLocalMemorySupportedAndReinit(true);
    pKmdSysManager->pciCurrentLinkSpeedReturnCode = KmdSysman::KmdSysmanFail;
    pKmdSysManager->pciCurrentLinkWidthReturnCode = KmdSysman::KmdSysmanFail;
    zes_pci_state_t pState = {};
    pState.speed.gen = -1;
    pState.speed.width = -1;
    WddmPciImp *pPciImp = new WddmPciImp(pOsSysman);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pPciImp->getState(&pState));
    EXPECT_EQ(pState.speed.gen, -1);
    EXPECT_EQ(pState.speed.gen, -1);
    delete pPciImp;
}

TEST_F(SysmanDevicePciFixture, WhenConvertingLinkSpeedThenResultIsCorrect) {
    for (int32_t i = PciGenerations::PciGen1; i <= PciGenerations::PciGen5; i++) {
        double speed = convertPciGenToLinkSpeed(i);
        int32_t gen = convertLinkSpeedToPciGen(speed);
        EXPECT_EQ(i, gen);
    }

    EXPECT_EQ(-1, convertLinkSpeedToPciGen(0.0));
    EXPECT_EQ(0.0, convertPciGenToLinkSpeed(0));
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenGettingResizableBarSupportAndRequestSingleFailsThenUnknownValuesAreReturned) {
    setLocalMemorySupportedAndReinit(true);
    pKmdSysManager->mockRequestSingle = true;
    pKmdSysManager->mockRequestSingleResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    WddmPciImp *pPciImp = new WddmPciImp(pOsSysman);
    EXPECT_EQ(false, pPciImp->resizableBarSupported());
    delete pPciImp;
}

TEST_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenGettingResizableBarEnabledAndRequestSingleFailsThenUnknownValuesAreReturned) {
    setLocalMemorySupportedAndReinit(true);
    pKmdSysManager->mockRequestSingle = true;
    pKmdSysManager->mockRequestSingleResult = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    uint32_t barIndex = 1;
    WddmPciImp *pPciImp = new WddmPciImp(pOsSysman);
    EXPECT_EQ(false, pPciImp->resizableBarEnabled(barIndex));
    delete pPciImp;
}

HWTEST2_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallingGetPropertiesThenPropertiesAreSetToTrue, IsBMG) {
    zes_pci_properties_t properties{};
    WddmPciImp *pPciImp = new WddmPciImp(pOsSysman);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pPciImp->getProperties(&properties));
    EXPECT_TRUE(properties.haveBandwidthCounters);
    EXPECT_TRUE(properties.havePacketCounters);
    EXPECT_TRUE(properties.haveReplayCounters);
    delete pPciImp;
}

HWTEST2_F(SysmanDevicePciFixture, GivenValidSysmanHandleWhenCallingGetPropertiesThenPropertiesAreSetToFalse, IsNotBMG) {
    zes_pci_properties_t properties{};
    WddmPciImp *pPciImp = new WddmPciImp(pOsSysman);
    EXPECT_EQ(ZE_RESULT_SUCCESS, pPciImp->getProperties(&properties));
    EXPECT_FALSE(properties.haveBandwidthCounters);
    EXPECT_FALSE(properties.havePacketCounters);
    EXPECT_FALSE(properties.haveReplayCounters);
    delete pPciImp;
}

HWTEST2_F(SysmanDevicePciFixture, GivenValidDeviceHandleWhenGettingZesDevicePciGetStatsThenValidReadingIsRetrieved, IsBMG) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, [](LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) -> HANDLE {
        return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x7));
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        PmtSysman::PmtTelemetryRead *readRequest = static_cast<PmtSysman::PmtTelemetryRead *>(lpInBuffer);
        switch (readRequest->offset) {
        case 70:
            *lpBytesReturned = 4;
            *static_cast<uint32_t *>(lpOutBuffer) = mockRxCounter;
            return true;
        case 72:
            *lpBytesReturned = 4;
            *static_cast<uint32_t *>(lpOutBuffer) = mockTxCounter;
            return true;
        case 74:
            *lpBytesReturned = 4;
            *static_cast<uint32_t *>(lpOutBuffer) = mockRxPacketCounter;
            return true;
        case 76:
            *lpBytesReturned = 4;
            *static_cast<uint32_t *>(lpOutBuffer) = mockTxPacketCounter;
            return true;
        case 93:
            *lpBytesReturned = 8;
            *static_cast<uint32_t *>(lpOutBuffer) = mockTimestamp;
            return true;
        default:
            *lpBytesReturned = 4;
            *static_cast<uint32_t *>(lpOutBuffer) = 0;
            return true;
        }
        return false;
    });
    zes_pci_stats_t stats;
    ze_result_t result = zesDevicePciGetStats(pSysmanDevice->toHandle(), &stats);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(stats.rxCounter, mockRxCounter);
    EXPECT_EQ(stats.txCounter, mockTxCounter);
    EXPECT_EQ(stats.packetCounter, mockRxPacketCounter + mockTxPacketCounter);
    EXPECT_EQ(stats.timestamp, mockTimestamp * milliSecsToMicroSecs);
}

HWTEST2_F(SysmanDevicePciFixture, GivenNullPmtHandleWhenGettingZesDevicePciGetStatsThenCallFails, IsBMG) {
    pWddmSysmanImp->pPmt.reset(nullptr);
    zes_pci_stats_t stats;
    ze_result_t result = zesDevicePciGetStats(pSysmanDevice->toHandle(), &stats);
    ASSERT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

HWTEST2_F(SysmanDevicePciFixture, GivenValidPmtHandleWhenCallingZesDevicePciGetStatsAndIoctlFailsThenCallsFails, IsBMG) {
    static int count = 10;
    VariableBackup<decltype(NEO::SysCalls::sysCallsCreateFile)> psysCallsCreateFile(&NEO::SysCalls::sysCallsCreateFile, [](LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) -> HANDLE {
        return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x7));
    });
    VariableBackup<decltype(NEO::SysCalls::sysCallsDeviceIoControl)> psysCallsDeviceIoControl(&NEO::SysCalls::sysCallsDeviceIoControl, [](HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) -> BOOL {
        PmtSysman::PmtTelemetryRead *readRequest = static_cast<PmtSysman::PmtTelemetryRead *>(lpInBuffer);
        switch (readRequest->offset) {
        case 69:
            *lpBytesReturned = 4;
            *static_cast<uint32_t *>(lpOutBuffer) = 0;
            return count == 1 ? false : true;
        case 70:
            *lpBytesReturned = 4;
            *static_cast<uint32_t *>(lpOutBuffer) = mockRxCounter;
            return count == 2 ? false : true;
        case 71:
            *lpBytesReturned = 4;
            *static_cast<uint32_t *>(lpOutBuffer) = 9;
            return count == 3 ? false : true;
        case 72:
            *lpBytesReturned = 4;
            *static_cast<uint32_t *>(lpOutBuffer) = mockTxCounter;
            return count == 4 ? false : true;
        case 73:
            *lpBytesReturned = 4;
            *static_cast<uint32_t *>(lpOutBuffer) = 0;
            return count == 5 ? false : true;
        case 74:
            *lpBytesReturned = 4;
            *static_cast<uint32_t *>(lpOutBuffer) = mockRxPacketCounter;
            return count == 6 ? false : true;
        case 75:
            *lpBytesReturned = 4;
            *static_cast<uint32_t *>(lpOutBuffer) = 0;
            return count == 7 ? false : true;
        case 76:
            *lpBytesReturned = 4;
            *static_cast<uint32_t *>(lpOutBuffer) = mockTxPacketCounter;
            return count == 8 ? false : true;
        case 92:
            *lpBytesReturned = 4;
            *static_cast<uint32_t *>(lpOutBuffer) = 0;
            return count == 9 ? false : true;
        case 93:
            *lpBytesReturned = 4;
            *static_cast<uint32_t *>(lpOutBuffer) = mockTimestamp;
            return count == 10 ? false : true;
        }
        return false;
    });
    zes_pci_stats_t stats;
    while (count) {
        ze_result_t result = zesDevicePciGetStats(pSysmanDevice->toHandle(), &stats);
        ASSERT_EQ(ZE_RESULT_ERROR_UNKNOWN, result);
        count--;
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
