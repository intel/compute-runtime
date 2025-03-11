/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm/adapter_info.h"

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/windows/gdi_interface.h"

namespace NEO {
std::wstring queryAdapterDriverStorePath(const Gdi &gdi, D3DKMT_HANDLE adapter) {
    D3DDDI_QUERYREGISTRY_INFO queryRegistryInfoSizeDesc = {};
    queryRegistryInfoSizeDesc.QueryType = D3DDDI_QUERYREGISTRY_DRIVERSTOREPATH;
    queryRegistryInfoSizeDesc.ValueType = 0;
    queryRegistryInfoSizeDesc.PhysicalAdapterIndex = 0;

    D3DKMT_QUERYADAPTERINFO queryAdapterInfoDesc = {0};
    queryAdapterInfoDesc.hAdapter = adapter;
    queryAdapterInfoDesc.Type = KMTQAITYPE_QUERYREGISTRY;
    queryAdapterInfoDesc.pPrivateDriverData = &queryRegistryInfoSizeDesc;
    queryAdapterInfoDesc.PrivateDriverDataSize = static_cast<UINT>(sizeof(queryRegistryInfoSizeDesc));

    NTSTATUS status = gdi.queryAdapterInfo(&queryAdapterInfoDesc);
    UNRECOVERABLE_IF(status != STATUS_SUCCESS);
    DEBUG_BREAK_IF(queryRegistryInfoSizeDesc.Status != D3DDDI_QUERYREGISTRY_STATUS_BUFFER_OVERFLOW);

    const auto privateDataSizeNeeded = queryRegistryInfoSizeDesc.OutputValueSize + sizeof(D3DDDI_QUERYREGISTRY_INFO);
    auto storage = std::make_unique_for_overwrite<uint64_t[]>((privateDataSizeNeeded + sizeof(uint64_t) - 1) / sizeof(uint64_t));
    D3DDDI_QUERYREGISTRY_INFO &queryRegistryInfoValueDesc = *reinterpret_cast<D3DDDI_QUERYREGISTRY_INFO *>(storage.get());
    queryRegistryInfoValueDesc = {};
    queryRegistryInfoValueDesc.QueryType = D3DDDI_QUERYREGISTRY_DRIVERSTOREPATH;
    queryRegistryInfoValueDesc.ValueType = 0;
    queryRegistryInfoValueDesc.PhysicalAdapterIndex = 0;

    queryAdapterInfoDesc.pPrivateDriverData = &queryRegistryInfoValueDesc;
    queryAdapterInfoDesc.PrivateDriverDataSize = static_cast<UINT>(privateDataSizeNeeded);

    status = gdi.queryAdapterInfo(&queryAdapterInfoDesc);
    UNRECOVERABLE_IF(status != STATUS_SUCCESS);
    UNRECOVERABLE_IF(queryRegistryInfoValueDesc.Status != D3DDDI_QUERYREGISTRY_STATUS_SUCCESS);

    return std::wstring(std::wstring(queryRegistryInfoValueDesc.OutputString, queryRegistryInfoValueDesc.OutputString + queryRegistryInfoValueDesc.OutputValueSize / sizeof(wchar_t)).c_str());
}

} // namespace NEO
