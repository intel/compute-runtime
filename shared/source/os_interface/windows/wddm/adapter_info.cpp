/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm/adapter_info.h"

#include "shared/source/os_interface/windows/gdi_interface.h"

#include <algorithm>
#include <cwchar>
#include <iterator>
#include <string_view>
#include <vector>

namespace NEO {
namespace {

std::string queryAdapterRegistryString(const Gdi &gdi, D3DKMT_HANDLE adapter, D3DDDI_QUERYREGISTRY_TYPE queryType, std::wstring_view valueName) {
    constexpr ULONG regSzValueType = 1; // REG_SZ

    auto queryRegistry = [&gdi, adapter, queryType, valueName](D3DDDI_QUERYREGISTRY_INFO &registryInfo, size_t registryInfoSize) {
        registryInfo = {};
        registryInfo.QueryType = queryType;
        if (!valueName.empty()) {
            valueName.copy(registryInfo.ValueName, std::size(registryInfo.ValueName) - 1);
            registryInfo.ValueType = regSzValueType;
        }

        D3DKMT_QUERYADAPTERINFO queryAdapterInfo = {};
        queryAdapterInfo.hAdapter = adapter;
        queryAdapterInfo.Type = KMTQAITYPE_QUERYREGISTRY;
        queryAdapterInfo.pPrivateDriverData = &registryInfo;
        queryAdapterInfo.PrivateDriverDataSize = static_cast<UINT>(registryInfoSize);
        return gdi.queryAdapterInfo(&queryAdapterInfo) == STATUS_SUCCESS;
    };

    D3DDDI_QUERYREGISTRY_INFO sizeQuery = {};
    if (!queryRegistry(sizeQuery, sizeof(sizeQuery)) || sizeQuery.Status != D3DDDI_QUERYREGISTRY_STATUS_BUFFER_OVERFLOW) {
        return {};
    }

    const auto requiredSize = sizeof(D3DDDI_QUERYREGISTRY_INFO) + sizeQuery.OutputValueSize;
    std::vector<uint64_t> storage((requiredSize + sizeof(uint64_t) - 1) / sizeof(uint64_t));
    auto &valueQuery = *reinterpret_cast<D3DDDI_QUERYREGISTRY_INFO *>(storage.data());
    if (!queryRegistry(valueQuery, requiredSize) || valueQuery.Status != D3DDDI_QUERYREGISTRY_STATUS_SUCCESS) {
        return {};
    }

    const auto length = wcsnlen(valueQuery.OutputString, valueQuery.OutputValueSize / sizeof(wchar_t));
    std::string value;
    value.reserve(length);
    std::transform(valueQuery.OutputString, valueQuery.OutputString + length, std::back_inserter(value), [](wchar_t c) { return static_cast<char>(c); });
    return value;
}

} // namespace

std::string queryAdapterDriverStorePath(const Gdi &gdi, D3DKMT_HANDLE adapter) {
    return queryAdapterRegistryString(gdi, adapter, D3DDDI_QUERYREGISTRY_DRIVERSTOREPATH, {});
}

std::string queryAdapterRegistry(const Gdi &gdi, D3DKMT_HANDLE adapter, std::wstring_view valueName) {
    return queryAdapterRegistryString(gdi, adapter, D3DDDI_QUERYREGISTRY_ADAPTERKEY, valueName);
}

} // namespace NEO
