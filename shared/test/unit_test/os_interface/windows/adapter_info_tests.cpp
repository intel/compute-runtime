/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/os_interface/windows/adapter_info_tests.h"

#include "shared/source/os_interface/windows/wddm/adapter_factory.h"
#include "shared/source/os_interface/windows/wddm/adapter_factory_dxcore.h"
#include "shared/source/os_interface/windows/wddm/adapter_factory_dxgi.h"
#include "shared/source/os_interface/windows/wddm/adapter_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/windows/ult_dxcore_factory.h"
#include "shared/test/common/os_interface/windows/ult_dxgi_factory.h"
#include "shared/test/common/test_macros/test.h"

#include <memory>

namespace {

enum class DriverStorePathQueryFailure {
    firstCall,
    firstStatus,
    secondCall,
    secondStatus
};

DriverStorePathQueryFailure queryFailure = DriverStorePathQueryFailure::firstCall;
uint32_t queryCallCount = 0;

NTSTATUS APIENTRY queryAdapterInfoWithFailure(const D3DKMT_QUERYADAPTERINFO *queryAdapterInfo) {
    auto &queryRegistryInfo = *reinterpret_cast<D3DDDI_QUERYREGISTRY_INFO *>(queryAdapterInfo->pPrivateDriverData);
    ++queryCallCount;

    if (queryCallCount == 1) {
        if (queryFailure == DriverStorePathQueryFailure::firstCall) {
            return STATUS_UNSUCCESSFUL;
        }
        queryRegistryInfo.OutputValueSize = static_cast<ULONG>(std::wstring(driverStorePathStr).size() * sizeof(wchar_t));
        queryRegistryInfo.Status = queryFailure == DriverStorePathQueryFailure::firstStatus
                                       ? D3DDDI_QUERYREGISTRY_STATUS_SUCCESS
                                       : D3DDDI_QUERYREGISTRY_STATUS_BUFFER_OVERFLOW;
        return STATUS_SUCCESS;
    }

    if (queryFailure == DriverStorePathQueryFailure::secondCall) {
        return STATUS_UNSUCCESSFUL;
    }
    queryRegistryInfo.Status = queryFailure == DriverStorePathQueryFailure::secondStatus
                                   ? D3DDDI_QUERYREGISTRY_STATUS_BUFFER_OVERFLOW
                                   : D3DDDI_QUERYREGISTRY_STATUS_SUCCESS;
    return STATUS_SUCCESS;
}

NTSTATUS APIENTRY queryAdapterInfoWithNullTerminatedPath(const D3DKMT_QUERYADAPTERINFO *queryAdapterInfo) {
    auto &queryRegistryInfo = *reinterpret_cast<D3DDDI_QUERYREGISTRY_INFO *>(queryAdapterInfo->pPrivateDriverData);
    const std::wstring driverStorePath(driverStorePathStr);
    queryRegistryInfo.OutputValueSize = static_cast<ULONG>((driverStorePath.size() + 1) * sizeof(wchar_t));
    if (queryAdapterInfo->PrivateDriverDataSize < sizeof(D3DDDI_QUERYREGISTRY_INFO) + queryRegistryInfo.OutputValueSize) {
        queryRegistryInfo.Status = D3DDDI_QUERYREGISTRY_STATUS_BUFFER_OVERFLOW;
        return STATUS_SUCCESS;
    }

    memcpy_s(queryRegistryInfo.OutputString, queryRegistryInfo.OutputValueSize, driverStorePath.c_str(), queryRegistryInfo.OutputValueSize);
    queryRegistryInfo.Status = D3DDDI_QUERYREGISTRY_STATUS_SUCCESS;
    return STATUS_SUCCESS;
}

const wchar_t *computeRuntimePathStr = L"C:\\local\\compute_runtime";
const wchar_t *openClDriverNameStr = L"C:\\local\\compute_runtime\\igdrcl64.dll";

NTSTATUS APIENTRY queryAdapterInfoWithComputeRuntimePath(const D3DKMT_QUERYADAPTERINFO *queryAdapterInfo) {
    constexpr ULONG regSzValueType = 1; // REG_SZ
    auto &queryRegistryInfo = *reinterpret_cast<D3DDDI_QUERYREGISTRY_INFO *>(queryAdapterInfo->pPrivateDriverData);
    if ((KMTQAITYPE_QUERYREGISTRY != queryAdapterInfo->Type) ||
        (D3DDDI_QUERYREGISTRY_ADAPTERKEY != queryRegistryInfo.QueryType) ||
        (regSzValueType != queryRegistryInfo.ValueType)) {
        return STATUS_INVALID_PARAMETER;
    }
    const std::wstring valueName(queryRegistryInfo.ValueName);
    const wchar_t *valueStr = nullptr;
    if (valueName == L"DriverStorePathForComputeRuntime") {
        valueStr = computeRuntimePathStr;
    } else if (valueName == L"OpenCLDriverName") {
        valueStr = openClDriverNameStr;
    } else {
        return STATUS_INVALID_PARAMETER;
    }
    if (nullptr == valueStr) {
        return STATUS_UNSUCCESSFUL;
    }

    const std::wstring computeRuntimePath(valueStr);
    queryRegistryInfo.OutputValueSize = static_cast<ULONG>((computeRuntimePath.size() + 1) * sizeof(wchar_t));
    if (queryAdapterInfo->PrivateDriverDataSize < sizeof(D3DDDI_QUERYREGISTRY_INFO) + queryRegistryInfo.OutputValueSize) {
        queryRegistryInfo.Status = D3DDDI_QUERYREGISTRY_STATUS_BUFFER_OVERFLOW;
        return STATUS_SUCCESS;
    }

    memcpy_s(queryRegistryInfo.OutputString, queryRegistryInfo.OutputValueSize, computeRuntimePath.c_str(), queryRegistryInfo.OutputValueSize);
    queryRegistryInfo.Status = D3DDDI_QUERYREGISTRY_STATUS_SUCCESS;
    return STATUS_SUCCESS;
}

} // namespace

HRESULT(WINAPI failCreateTopLevelInterface)
(REFIID riid, void **ppFactory) {
    return -1;
}

template <typename T>
HRESULT(WINAPI createTopLevelInterface)(REFIID riid, void **ppFactory) {
    auto obj = std::make_unique<T>();
    *reinterpret_cast<T **>(ppFactory) = obj.get();
    obj.release();
    return S_OK;
}

TEST(AdapterInfoTest, whenQueryingForDriverStorePathThenProvidesEnoughMemoryForReturnString) {
    NEO::Gdi gdi;
    auto handle = validHandle;
    gdi.queryAdapterInfo.mFunc = QueryAdapterInfoMock::queryadapterinfo;

    auto ret = NEO::queryAdapterDriverStorePath(gdi, handle);

    EXPECT_EQ(std::string(expectedDriverStorePathStr), ret);
}

TEST(AdapterInfoTest, givenDriverStorePathWithNullTerminatorInOutputValueSizeWhenQueryingDriverStorePathThenTerminatorIsNotPartOfPath) {
    NEO::Gdi gdi;
    gdi.queryAdapterInfo.mFunc = queryAdapterInfoWithNullTerminatedPath;

    auto driverStorePath = NEO::queryAdapterDriverStorePath(gdi, validHandle);

    EXPECT_EQ(std::string(expectedDriverStorePathStr), driverStorePath);
}

TEST(AdapterInfoTest, givenAdapterWithComputeRuntimePathInRegistryWhenQueryingAdapterRegistryThenPathWithoutTerminatorIsReturned) {
    NEO::Gdi gdi;
    gdi.queryAdapterInfo.mFunc = queryAdapterInfoWithComputeRuntimePath;

    auto computeRuntimePath = NEO::queryAdapterRegistry(gdi, validHandle, L"DriverStorePathForComputeRuntime");

    EXPECT_EQ(std::string("C:\\local\\compute_runtime"), computeRuntimePath);
}

TEST(AdapterInfoTest, givenAdapterWithoutComputeRuntimePathInRegistryWhenQueryingAdapterRegistryThenNoPathIsReturned) {
    VariableBackup<const wchar_t *> computeRuntimePathBackup{&computeRuntimePathStr, nullptr};
    NEO::Gdi gdi;
    gdi.queryAdapterInfo.mFunc = queryAdapterInfoWithComputeRuntimePath;

    EXPECT_TRUE(NEO::queryAdapterRegistry(gdi, validHandle, L"DriverStorePathForComputeRuntime").empty());
}

TEST(AdapterInfoTest, givenAdapterWithOpenClDriverNameInRegistryWhenQueryingAdapterRegistryThenNameIsReturned) {
    NEO::Gdi gdi;
    gdi.queryAdapterInfo.mFunc = queryAdapterInfoWithComputeRuntimePath;

    EXPECT_EQ(std::string("C:\\local\\compute_runtime\\igdrcl64.dll"), NEO::queryAdapterRegistry(gdi, validHandle, L"OpenCLDriverName"));
}

TEST(AdapterInfoTest, givenAdapterWithoutOpenClDriverNameInRegistryWhenQueryingAdapterRegistryThenEmptyNameIsReturned) {
    VariableBackup<const wchar_t *> openClDriverNameBackup{&openClDriverNameStr, nullptr};
    NEO::Gdi gdi;
    gdi.queryAdapterInfo.mFunc = queryAdapterInfoWithComputeRuntimePath;

    EXPECT_TRUE(NEO::queryAdapterRegistry(gdi, validHandle, L"OpenCLDriverName").empty());
}

class AdapterDriverStorePathFailureTest : public ::testing::TestWithParam<DriverStorePathQueryFailure> {
  public:
    void SetUp() override {
        queryFailure = GetParam();
        queryCallCount = 0;
        gdi.queryAdapterInfo.mFunc = queryAdapterInfoWithFailure;
    }

    NEO::Gdi gdi;
};

TEST_P(AdapterDriverStorePathFailureTest, givenFailingDriverStorePathQueryWhenQueryingDriverStorePathThenNoPathIsReturned) {
    EXPECT_TRUE(NEO::queryAdapterDriverStorePath(gdi, validHandle).empty());
}

INSTANTIATE_TEST_SUITE_P(
    AdapterInfoTest,
    AdapterDriverStorePathFailureTest,
    ::testing::Values(DriverStorePathQueryFailure::firstCall,
                      DriverStorePathQueryFailure::firstStatus,
                      DriverStorePathQueryFailure::secondCall,
                      DriverStorePathQueryFailure::secondStatus));

TEST(DxCoreAdapterFactory, whenCouldNotCreateAdapterFactoryThenReturnsUnsupported) {
    {
        NEO::DxCoreAdapterFactory adapterFactor{nullptr};
        EXPECT_FALSE(adapterFactor.isSupported());
    }

    {
        NEO::DxCoreAdapterFactory adapterFactory{failCreateTopLevelInterface};
        EXPECT_FALSE(adapterFactory.isSupported());
    }
}

TEST(DxCoreAdapterFactory, whenCreatedAdapterFactoryThenReturnsSupported) {
    NEO::DxCoreAdapterFactory adapterFactory{&createTopLevelInterface<NEO::UltDXCoreAdapterFactory>};
    EXPECT_TRUE(adapterFactory.isSupported());
}

TEST(DxCoreAdapterFactory, whenSupportedButSnapshotOfAdaptersNotCreatedThenReturnsEmptyListOfAdapters) {
    NEO::DxCoreAdapterFactory adapterFactory{&createTopLevelInterface<NEO::UltDXCoreAdapterFactory>};
    EXPECT_EQ(0U, adapterFactory.getNumAdaptersInSnapshot());

    NEO::AdapterFactory::AdapterDesc adapterDesc;
    EXPECT_FALSE(adapterFactory.getAdapterDesc(0, adapterDesc));
}

TEST(DxCoreAdapterFactory, whenSupportedThenGiveAccessToUnderlyingAdapterDesc) {
    NEO::DxCoreAdapterFactory adapterFactory{&createTopLevelInterface<NEO::UltDXCoreAdapterFactory>};

    bool createdSnapshot = adapterFactory.createSnapshotOfAvailableAdapters();
    EXPECT_TRUE(createdSnapshot);
    EXPECT_EQ(1U, adapterFactory.getNumAdaptersInSnapshot());

    NEO::AdapterFactory::AdapterDesc adapterDesc;
    bool retrievedAdapterDesc = adapterFactory.getAdapterDesc(0, adapterDesc);
    EXPECT_TRUE(retrievedAdapterDesc);
    EXPECT_EQ(NEO::AdapterFactory::AdapterDesc::Type::hardware, adapterDesc.type);
    EXPECT_EQ(0x1234U, adapterDesc.deviceId);
    EXPECT_EQ(0x1234, adapterDesc.luid.HighPart);
    EXPECT_EQ(0U, adapterDesc.luid.LowPart);
    EXPECT_STREQ("Intel", adapterDesc.driverDescription.c_str());
}

TEST(DxgiAdapterFactory, whenCouldNotCreateAdapterFactoryThenReturnsUnsupported) {
    {
        NEO::DxgiAdapterFactory adapterFactory{nullptr};
        EXPECT_FALSE(adapterFactory.isSupported());
    }

    {
        NEO::DxgiAdapterFactory adapterFactory{failCreateTopLevelInterface};
        EXPECT_FALSE(adapterFactory.isSupported());
    }
}

TEST(DxgiAdapterFactory, whenCreatedAdapterFactoryThenReturnsSupported) {
    NEO::DxgiAdapterFactory adapterFactory{&createTopLevelInterface<NEO::UltIDXGIFactory1>};
    EXPECT_TRUE(adapterFactory.isSupported());
}

TEST(DxgiAdapterFactory, whenSupportedButSnapshotOfAdaptersNotCreatedThenReturnsEmptyListOfAdapters) {
    NEO::DxgiAdapterFactory adapterFactory{&createTopLevelInterface<NEO::UltIDXGIFactory1>};
    EXPECT_EQ(0U, adapterFactory.getNumAdaptersInSnapshot());

    NEO::AdapterFactory::AdapterDesc adapterDesc;
    EXPECT_FALSE(adapterFactory.getAdapterDesc(0, adapterDesc));
}

TEST(DxgiAdapterFactory, whenSupportedThenGiveAccessToUnderlyingAdapterDesc) {
    NEO::DxgiAdapterFactory adapterFactory{&createTopLevelInterface<NEO::UltIDXGIFactory1>};

    bool createdSnapshot = adapterFactory.createSnapshotOfAvailableAdapters();
    EXPECT_TRUE(createdSnapshot);
    EXPECT_EQ(1U, adapterFactory.getNumAdaptersInSnapshot());

    NEO::AdapterFactory::AdapterDesc adapterDesc;
    bool retrievedAdapterDesc = adapterFactory.getAdapterDesc(0, adapterDesc);
    EXPECT_TRUE(retrievedAdapterDesc);
    EXPECT_EQ(NEO::AdapterFactory::AdapterDesc::Type::unknown, adapterDesc.type);
    EXPECT_EQ(0x1234U, adapterDesc.deviceId);
    EXPECT_EQ(0x1234, adapterDesc.luid.HighPart);
    EXPECT_EQ(0U, adapterDesc.luid.LowPart);
    EXPECT_STREQ("Intel", adapterDesc.driverDescription.c_str());
}

TEST(IsAllowedDeviceId, whenDebugKeyNotSetThenReturnTrue) {
    EXPECT_TRUE(NEO::isAllowedDeviceId(0xdeadbeef));
}

TEST(IsAllowedDeviceId, whenFilterDeviceIdDebugKeySetThenExpectSpecificValue) {
    DebugManagerStateRestore rest;
    debugManager.flags.FilterDeviceId.set("167");
    EXPECT_FALSE(NEO::isAllowedDeviceId(0xdeadbeef));

    debugManager.flags.FilterDeviceId.set("1678");
    EXPECT_FALSE(NEO::isAllowedDeviceId(0x167));

    debugManager.flags.FilterDeviceId.set("167");
    EXPECT_TRUE(NEO::isAllowedDeviceId(0x167));
}

TEST(IsAllowedDeviceId, whenFilterDeviceIdDebugKeySetThenTreatAsHex) {
    DebugManagerStateRestore rest;

    debugManager.flags.FilterDeviceId.set("167");
    EXPECT_FALSE(NEO::isAllowedDeviceId(167));

    debugManager.flags.FilterDeviceId.set("167");
    EXPECT_TRUE(NEO::isAllowedDeviceId(0x167));
}
