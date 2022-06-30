/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/wddm/adapter_info.h"

static constexpr D3DKMT_HANDLE validHandle = 0x7;
static constexpr auto error = STATUS_SUCCESS + 1;
static const wchar_t *driverStorePathStr = L"some/path/fffff";
struct QueryAdapterInfoMock {
    static NTSTATUS(APIENTRY queryadapterinfo)(
        const D3DKMT_QUERYADAPTERINFO *queryAdapterInfo) {
        if (nullptr == queryAdapterInfo) {
            return error;
        }
        if (KMTQAITYPE_QUERYREGISTRY != queryAdapterInfo->Type) {
            return error;
        }

        if ((nullptr == queryAdapterInfo->pPrivateDriverData) || (sizeof(D3DDDI_QUERYREGISTRY_INFO) > queryAdapterInfo->PrivateDriverDataSize)) {
            return error;
        }

        D3DDDI_QUERYREGISTRY_INFO *queryRegistryInfo = reinterpret_cast<D3DDDI_QUERYREGISTRY_INFO *>(queryAdapterInfo->pPrivateDriverData);
        if (D3DDDI_QUERYREGISTRY_DRIVERSTOREPATH != queryRegistryInfo->QueryType) {
            return error;
        }
        if (0U != queryRegistryInfo->QueryFlags.Value) {
            return error;
        }
        bool regValueNameIsEmpty = std::wstring(std::wstring(queryRegistryInfo->ValueName[0], queryRegistryInfo->ValueName[0] + sizeof(queryRegistryInfo->ValueName)).c_str()).empty();
        if (false == regValueNameIsEmpty) {
            return error;
        }
        if (0U != queryRegistryInfo->ValueType) {
            return error;
        }
        if (0U != queryRegistryInfo->PhysicalAdapterIndex) {
            return error;
        }

        if (D3DDDI_QUERYREGISTRY_DRIVERSTOREPATH != queryRegistryInfo->QueryType) {
            return error;
        }

        queryRegistryInfo->OutputValueSize = static_cast<ULONG>(std::wstring(driverStorePathStr).size() * sizeof(wchar_t));
        if (queryAdapterInfo->PrivateDriverDataSize < queryRegistryInfo->OutputValueSize + sizeof(D3DDDI_QUERYREGISTRY_INFO)) {
            queryRegistryInfo->Status = D3DDDI_QUERYREGISTRY_STATUS_BUFFER_OVERFLOW;
            return STATUS_SUCCESS;
        }

        memcpy_s(queryRegistryInfo->OutputString, queryAdapterInfo->PrivateDriverDataSize - sizeof(D3DDDI_QUERYREGISTRY_INFO),
                 driverStorePathStr, queryRegistryInfo->OutputValueSize);

        queryRegistryInfo->Status = D3DDDI_QUERYREGISTRY_STATUS_SUCCESS;
        return STATUS_SUCCESS;
    }
};
