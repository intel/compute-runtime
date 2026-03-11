/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_library.h"
#include "shared/source/os_interface/windows/hw_device_id.h"
#include "shared/source/os_interface/windows/os_environment_win.h"
#include "shared/source/os_interface/windows/pdh_interface_win.h"
#include "shared/source/os_interface/windows/wddm/um_km_data_translator.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mock_gdi/mock_gdi.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/common/test_macros/test.h"

#include <array>
#include <memory>
#include <unordered_map>

using namespace NEO;

namespace {
static inline PDH_HQUERY pdhExpectedQuery = reinterpret_cast<PDH_HQUERY>(0xABC);
static inline LONGLONG pdhDedicatedValue = 0;
static inline LONGLONG pdhSharedValue = 0;
static inline PDH_STATUS pdhOpenQueryStatus = ERROR_SUCCESS;
static inline PDH_STATUS pdhAddEnglishCounterStatus = ERROR_SUCCESS;
static inline PDH_STATUS pdhCollectQueryDataStatus = ERROR_SUCCESS;
static inline PDH_STATUS pdhGetFormattedCounterValueStatus = ERROR_SUCCESS;
static inline bool pdhLibraryLoadSuccess = true;
static inline bool pdhLibraryIsLoaded = true;
static inline std::string pdhMissingProcName = "";
static inline uint32_t pdhAddCounterCallCount = 0;
static inline uint32_t pdhCloseQueryCallCount = 0;
static inline std::vector<std::wstring> pdhAddedCounterPaths;
static inline std::unordered_map<PDH_HCOUNTER, LONGLONG> pdhCounterValues;
static inline PDH_HCOUNTER pdhNextCounterHandle = reinterpret_cast<PDH_HCOUNTER>(0x1000);

static PDH_STATUS __stdcall pdhCloseQuerySuccessMock(PDH_HQUERY) {
    pdhCloseQueryCallCount++;
    return ERROR_SUCCESS;
}

static void resetPdhMockState() {
    pdhOpenQueryStatus = ERROR_SUCCESS;
    pdhAddEnglishCounterStatus = ERROR_SUCCESS;
    pdhCollectQueryDataStatus = ERROR_SUCCESS;
    pdhGetFormattedCounterValueStatus = ERROR_SUCCESS;
    pdhLibraryLoadSuccess = true;
    pdhLibraryIsLoaded = true;
    pdhMissingProcName = "";
    pdhAddCounterCallCount = 0;
    pdhCloseQueryCallCount = 0;
    pdhDedicatedValue = 0;
    pdhSharedValue = 0;
    pdhCounterValues.clear();
    pdhNextCounterHandle = reinterpret_cast<PDH_HCOUNTER>(0x1000);
    std::vector<std::wstring>().swap(pdhAddedCounterPaths);
}

static PDH_STATUS __stdcall pdhOpenQueryConfigurableMock(LPCWSTR, DWORD_PTR, PDH_HQUERY *query) {
    if (pdhOpenQueryStatus == ERROR_SUCCESS) {
        *query = pdhExpectedQuery;
    }
    return pdhOpenQueryStatus;
}

static PDH_STATUS __stdcall pdhAddEnglishCounterConfigurableMock(PDH_HQUERY, LPCWSTR counterPath, DWORD_PTR, PDH_HCOUNTER *counter) {
    pdhAddCounterCallCount++;
    if (pdhAddEnglishCounterStatus != ERROR_SUCCESS) {
        return pdhAddEnglishCounterStatus;
    }

    const std::wstring path = counterPath ? counterPath : L"";
    pdhAddedCounterPaths.push_back(path);

    *counter = pdhNextCounterHandle;
    pdhNextCounterHandle = reinterpret_cast<PDH_HCOUNTER>(reinterpret_cast<uintptr_t>(pdhNextCounterHandle) + 1);

    if (path.find(L"Dedicated Usage") != std::wstring::npos) {
        pdhCounterValues[*counter] = pdhDedicatedValue;
    } else {
        pdhCounterValues[*counter] = pdhSharedValue;
    }
    return ERROR_SUCCESS;
}

static PDH_STATUS __stdcall pdhCollectQueryDataConfigurableMock(PDH_HQUERY) {
    return pdhCollectQueryDataStatus;
}

static PDH_STATUS __stdcall pdhGetFormattedCounterValueConfigurableMock(PDH_HCOUNTER counter, DWORD, LPDWORD, PPDH_FMT_COUNTERVALUE value) {
    if (pdhGetFormattedCounterValueStatus != ERROR_SUCCESS) {
        return pdhGetFormattedCounterValueStatus;
    }
    value->CStatus = ERROR_SUCCESS;

    auto it = pdhCounterValues.find(counter);
    EXPECT_NE(pdhCounterValues.end(), it) << "Counter not found in map - test must add counter via normal flow";
    value->largeValue = it->second;
    return ERROR_SUCCESS;
}

class MockPdhOsLibrary : public NEO::OsLibrary {
  public:
    MockPdhOsLibrary() {
        procMap["PdhOpenQueryW"] = reinterpret_cast<void *>(&pdhOpenQueryConfigurableMock);
        procMap["PdhAddEnglishCounterW"] = reinterpret_cast<void *>(&pdhAddEnglishCounterConfigurableMock);
        procMap["PdhCollectQueryData"] = reinterpret_cast<void *>(&pdhCollectQueryDataConfigurableMock);
        procMap["PdhGetFormattedCounterValue"] = reinterpret_cast<void *>(&pdhGetFormattedCounterValueConfigurableMock);
        procMap["PdhCloseQuery"] = reinterpret_cast<void *>(&pdhCloseQuerySuccessMock);
    }

    void *getProcAddress(const std::string &procName) override {
        if (pdhMissingProcName == procName) {
            return nullptr;
        }
        auto it = procMap.find(procName);
        if (it != procMap.end()) {
            return it->second;
        }
        return nullptr;
    }

    bool isLoaded() override {
        return loaded;
    }

    std::string getFullPath() override {
        return std::string();
    }

    static OsLibrary *load(const NEO::OsLibraryCreateProperties &properties) {
        if (!pdhLibraryLoadSuccess) {
            return nullptr;
        }
        auto lib = new MockPdhOsLibrary();
        lib->loaded = pdhLibraryIsLoaded;
        return lib;
    }

    bool loaded = true;
    std::unordered_map<std::string, void *> procMap;
};

class MockPdhInterfaceWindows : public PdhInterfaceWindows {
  public:
    using PdhInterfaceWindows::initPdh;
    using PdhInterfaceWindows::memoryUsageQuery;
    using PdhInterfaceWindows::pdhCloseQueryFunction;
    using PdhInterfaceWindows::pdhInitialized;
    using PdhInterfaceWindows::PdhInterfaceWindows;
    using PdhInterfaceWindows::pdhLibrary;
};

class PdhInterfaceWindowsTest : public ::testing::Test {
  public:
    void SetUp() override {
        executionEnvironment.prepareRootDeviceEnvironments(2u);
        for (auto i = 0u; i < executionEnvironment.rootDeviceEnvironments.size(); i++) {
            executionEnvironment.rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
            executionEnvironment.rootDeviceEnvironments[i]->initGmm();

            if (!executionEnvironment.osEnvironment.get()) {
                executionEnvironment.osEnvironment = std::make_unique<OsEnvironmentWin>();
            }

            executionEnvironment.rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
            LUID adapterLuid = {static_cast<DWORD>(0x1000 + i), static_cast<LONG>(i)};
            auto hwDeviceId = std::make_unique<HwDeviceIdWddm>(ADAPTER_HANDLE,
                                                               adapterLuid,
                                                               1u,
                                                               executionEnvironment.osEnvironment.get(),
                                                               std::make_unique<UmKmDataTranslator>());
            auto wddm = new WddmMock(std::move(hwDeviceId), *executionEnvironment.rootDeviceEnvironments[i].get());
            executionEnvironment.rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
        }
        resetPdhMockState();
        backupLoadFunc = std::make_unique<VariableBackup<decltype(NEO::OsLibrary::loadFunc)>>(&NEO::OsLibrary::loadFunc, MockPdhOsLibrary::load);
    }

    void TearDown() override {
        resetPdhMockState();
    }

    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<VariableBackup<decltype(NEO::OsLibrary::loadFunc)>> backupLoadFunc;
};

} // namespace

TEST_F(PdhInterfaceWindowsTest, givenMockedOsLibraryAndPdhWhenGettingCurrentUsedDedicatedMemoryWithMultiDeviceThenReturnsExpectedValue) {
    auto pdhInterface = std::make_unique<PdhInterfaceWindows>(executionEnvironment);

    const uint32_t rootDeviceIndex0 = 0u;
    const uint32_t rootDeviceIndex1 = 1u;
    const uint64_t memoryValue0 = 4 * MemoryConstants::gigaByte;
    const uint64_t memoryValue1 = 8 * MemoryConstants::gigaByte;

    // Test device 0
    pdhDedicatedValue = memoryValue0;
    auto result0 = pdhInterface->getCurrentMemoryUsage(rootDeviceIndex0, true);
    EXPECT_EQ(memoryValue0, result0);

    // Verify counter path for device 0
    ASSERT_EQ(1u, pdhAddedCounterPaths.size());
    auto wddm0 = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex0]->osInterface->getDriverModel()->as<Wddm>();
    auto luid0 = wddm0->getAdapterLuid();
    EXPECT_EQ(0x0u, static_cast<unsigned long>(luid0.HighPart));
    EXPECT_EQ(0x1000u, static_cast<unsigned long>(luid0.LowPart));
    std::wstring expectedPath0 = L"\\GPU Adapter Memory(luid_0X00000000_0X00001000_phys_0)\\Dedicated Usage";
    EXPECT_EQ(expectedPath0, pdhAddedCounterPaths[0]);

    // Test device 1
    pdhDedicatedValue = memoryValue1;
    auto result1 = pdhInterface->getCurrentMemoryUsage(rootDeviceIndex1, true);
    EXPECT_EQ(memoryValue1, result1);

    // Verify counter path for device 1
    ASSERT_EQ(2u, pdhAddedCounterPaths.size());
    auto wddm1 = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex1]->osInterface->getDriverModel()->as<Wddm>();
    auto luid1 = wddm1->getAdapterLuid();
    EXPECT_TRUE(luid0.HighPart != luid1.HighPart || luid0.LowPart != luid1.LowPart);
    EXPECT_EQ(0x1u, static_cast<unsigned long>(luid1.HighPart));
    EXPECT_EQ(0x1001u, static_cast<unsigned long>(luid1.LowPart));

    std::wstring expectedPath1 = L"\\GPU Adapter Memory(luid_0X00000001_0X00001001_phys_0)\\Dedicated Usage";
    EXPECT_EQ(expectedPath1, pdhAddedCounterPaths[1]);
    EXPECT_NE(expectedPath0, expectedPath1);
}

TEST_F(PdhInterfaceWindowsTest, givenMockedOsLibraryAndPdhWhenGettingCurrentUsedSharedMemoryThenReturnsExpectedValue) {
    auto pdhInterface = std::make_unique<PdhInterfaceWindows>(executionEnvironment);

    const uint32_t rootDeviceIndex = 0u;
    pdhSharedValue = 2 * MemoryConstants::gigaByte;

    EXPECT_EQ(static_cast<uint64_t>(pdhSharedValue), pdhInterface->getCurrentMemoryUsage(rootDeviceIndex, false));

    pdhInterface.reset();
}

TEST_F(PdhInterfaceWindowsTest, givenMockedOsLibraryAndPdhWhenCallingInitPdhWithDifferentInitializationStatesThenReturnsExpectedStatus) {

    {
        auto pdhInterface = std::make_unique<MockPdhInterfaceWindows>(executionEnvironment);
        pdhInterface->pdhInitialized = true;
        EXPECT_TRUE(pdhInterface->initPdh());
    }

    {
        auto pdhInterface = std::make_unique<MockPdhInterfaceWindows>(executionEnvironment);
        VariableBackup<bool> backupLibraryLoadSuccess{&pdhLibraryLoadSuccess, false};
        EXPECT_FALSE(pdhInterface->initPdh());
    }

    for (auto missingProcName : std::array<std::string, 5>{"PdhOpenQueryW", "PdhAddEnglishCounterW", "PdhCollectQueryData", "PdhGetFormattedCounterValue", "PdhCloseQuery"}) {
        auto pdhInterface = std::make_unique<MockPdhInterfaceWindows>(executionEnvironment);
        VariableBackup<std::string> backupMissingProcName(&pdhMissingProcName, missingProcName);
        EXPECT_FALSE(pdhInterface->initPdh());
    }

    {
        auto pdhInterface = std::make_unique<MockPdhInterfaceWindows>(executionEnvironment);
        VariableBackup<PDH_STATUS> backupOpenQueryStatus{&pdhOpenQueryStatus, ERROR_INVALID_FUNCTION};
        EXPECT_FALSE(pdhInterface->initPdh());
    }

    {
        auto pdhInterface = std::make_unique<MockPdhInterfaceWindows>(executionEnvironment);
        EXPECT_TRUE(pdhInterface->initPdh());
    }
}

TEST_F(PdhInterfaceWindowsTest, givenMockedOsLibraryAndPdhWhenDestroyingPdhInterfaceThenExpectCorrectBehaviourBasedOnInitializationState) {
    VariableBackup<uint32_t> backupCloseQueryCallCount{&pdhCloseQueryCallCount, 0};

    // Test: Both conditions true - pdhCloseQuery should be called
    {
        auto pdhInterface = std::make_unique<MockPdhInterfaceWindows>(executionEnvironment);
        pdhInterface->pdhInitialized = true;
        pdhInterface->memoryUsageQuery = pdhExpectedQuery;
        pdhInterface->pdhCloseQueryFunction = &pdhCloseQuerySuccessMock;
    }
    EXPECT_EQ(1u, pdhCloseQueryCallCount);

    // Test: pdhInitialized true but memoryUsageQuery nullptr - pdhCloseQuery should NOT be called
    {
        auto pdhInterface = std::make_unique<MockPdhInterfaceWindows>(executionEnvironment);
        pdhInterface->pdhInitialized = true;
        pdhInterface->memoryUsageQuery = nullptr;
        pdhInterface->pdhCloseQueryFunction = &pdhCloseQuerySuccessMock;
    }
    EXPECT_EQ(1u, pdhCloseQueryCallCount);
}

TEST_F(PdhInterfaceWindowsTest, givenMockedOsLibraryAndPdhWhenCallingGetCurrentMemoryUsageWithDifferentScenariosThenReturnsExpectedValue) {
    const uint32_t rootDeviceIndex = 0u;

    {
        auto pdhInterface = std::make_unique<PdhInterfaceWindows>(executionEnvironment);
        VariableBackup<bool> backupLibraryLoadSuccess{&pdhLibraryLoadSuccess, false};
        EXPECT_EQ(0u, pdhInterface->getCurrentMemoryUsage(rootDeviceIndex, true));
    }

    {
        auto pdhInterface = std::make_unique<PdhInterfaceWindows>(executionEnvironment);
        VariableBackup<PDH_STATUS> backupAddEnglishCounterStatus{&pdhAddEnglishCounterStatus, ERROR_INVALID_FUNCTION};
        EXPECT_EQ(0u, pdhInterface->getCurrentMemoryUsage(rootDeviceIndex, true));
    }

    {
        auto pdhInterface = std::make_unique<PdhInterfaceWindows>(executionEnvironment);
        pdhDedicatedValue = 1000;
        EXPECT_EQ(static_cast<uint64_t>(pdhDedicatedValue), pdhInterface->getCurrentMemoryUsage(rootDeviceIndex, true));
        VariableBackup<PDH_STATUS> backupCollectQueryDataStatus{&pdhCollectQueryDataStatus, ERROR_INVALID_FUNCTION};
        EXPECT_EQ(0u, pdhInterface->getCurrentMemoryUsage(rootDeviceIndex, true));
    }

    {
        auto pdhInterface = std::make_unique<PdhInterfaceWindows>(executionEnvironment);
        pdhDedicatedValue = 2000;
        EXPECT_EQ(static_cast<uint64_t>(pdhDedicatedValue), pdhInterface->getCurrentMemoryUsage(rootDeviceIndex, true));
        VariableBackup<PDH_STATUS> backupGetFormattedCounterValueStatus{&pdhGetFormattedCounterValueStatus, ERROR_INVALID_FUNCTION};
        EXPECT_EQ(0u, pdhInterface->getCurrentMemoryUsage(rootDeviceIndex, true));
    }

    {
        auto pdhInterface = std::make_unique<PdhInterfaceWindows>(executionEnvironment);
        pdhDedicatedValue = -1;
        EXPECT_EQ(0u, pdhInterface->getCurrentMemoryUsage(rootDeviceIndex, true));
    }

    {
        auto pdhInterface = std::make_unique<PdhInterfaceWindows>(executionEnvironment);
        pdhDedicatedValue = 987654321;
        EXPECT_EQ(static_cast<uint64_t>(pdhDedicatedValue), pdhInterface->getCurrentMemoryUsage(rootDeviceIndex, true));
    }
}

TEST_F(PdhInterfaceWindowsTest, givenMockedOsLibraryWhenLibraryIsNotLoadedThenInitPdhReturnsFalse) {
    auto pdhInterface = std::make_unique<MockPdhInterfaceWindows>(executionEnvironment);

    pdhInterface->pdhLibrary.reset();
    VariableBackup<bool> backupLibraryIsLoaded{&pdhLibraryIsLoaded, false};
    EXPECT_FALSE(pdhInterface->initPdh());
}
