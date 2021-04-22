/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/wddm/adapter_info.h"

#include "test.h"

TEST(AdapterInfoTest, whenQueryingForDriverStorePathThenProvidesEnoughMemoryForReturnString) {
    static constexpr D3DKMT_HANDLE validHandle = 0x7;
    static constexpr auto error = STATUS_SUCCESS + 1;
    static const wchar_t *driverStorePathStr = L"some/path/fffff";
    struct QueryAdapterInfoMock {
        static NTSTATUS(APIENTRY queryadapterinfo)(
            const D3DKMT_QUERYADAPTERINFO *queryAdapterInfo) {
            EXPECT_NE(nullptr, queryAdapterInfo);
            if (nullptr == queryAdapterInfo) {
                return error;
            }
            EXPECT_EQ(validHandle, queryAdapterInfo->hAdapter);
            EXPECT_EQ(KMTQAITYPE_QUERYREGISTRY, queryAdapterInfo->Type);
            if (KMTQAITYPE_QUERYREGISTRY != queryAdapterInfo->Type) {
                return error;
            }

            EXPECT_NE(nullptr, queryAdapterInfo->pPrivateDriverData);
            EXPECT_LE(sizeof(D3DDDI_QUERYREGISTRY_INFO), queryAdapterInfo->PrivateDriverDataSize);
            if ((nullptr == queryAdapterInfo->pPrivateDriverData) || (sizeof(D3DDDI_QUERYREGISTRY_INFO) > queryAdapterInfo->PrivateDriverDataSize)) {
                return error;
            }

            D3DDDI_QUERYREGISTRY_INFO *queryRegistryInfo = reinterpret_cast<D3DDDI_QUERYREGISTRY_INFO *>(queryAdapterInfo->pPrivateDriverData);
            EXPECT_EQ(D3DDDI_QUERYREGISTRY_DRIVERSTOREPATH, queryRegistryInfo->QueryType);
            EXPECT_EQ(0U, queryRegistryInfo->QueryFlags.Value);
            bool regValueNameIsEmpty = std::wstring(std::wstring(queryRegistryInfo->ValueName[0], queryRegistryInfo->ValueName[0] + sizeof(queryRegistryInfo->ValueName)).c_str()).empty();
            EXPECT_TRUE(regValueNameIsEmpty);
            EXPECT_EQ(0U, queryRegistryInfo->ValueType);
            EXPECT_EQ(0U, queryRegistryInfo->PhysicalAdapterIndex);

            if (D3DDDI_QUERYREGISTRY_DRIVERSTOREPATH != queryRegistryInfo->QueryType) {
                return error;
            }

            queryRegistryInfo->OutputValueSize = static_cast<ULONG>(std::wstring(driverStorePathStr).size() * sizeof(wchar_t));
            if (queryAdapterInfo->PrivateDriverDataSize < queryRegistryInfo->OutputValueSize + sizeof(D3DDDI_QUERYREGISTRY_INFO)) {
                EXPECT_EQ(sizeof(D3DDDI_QUERYREGISTRY_INFO), queryAdapterInfo->PrivateDriverDataSize);
                queryRegistryInfo->Status = D3DDDI_QUERYREGISTRY_STATUS_BUFFER_OVERFLOW;
                return STATUS_SUCCESS;
            }

            memcpy_s(queryRegistryInfo->OutputString, queryAdapterInfo->PrivateDriverDataSize - sizeof(D3DDDI_QUERYREGISTRY_INFO),
                     driverStorePathStr, queryRegistryInfo->OutputValueSize);

            queryRegistryInfo->Status = D3DDDI_QUERYREGISTRY_STATUS_SUCCESS;
            return STATUS_SUCCESS;
        }
    };
    NEO::Gdi gdi;
    auto handle = validHandle;
    gdi.queryAdapterInfo.mFunc = QueryAdapterInfoMock::queryadapterinfo;

    auto ret = NEO::queryAdapterDriverStorePath(gdi, handle);

    EXPECT_EQ(std::wstring(driverStorePathStr), ret) << "Expected " << driverStorePathStr << ", got : " << ret;
}
