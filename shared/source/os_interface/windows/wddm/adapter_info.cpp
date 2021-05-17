/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm/adapter_info.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/utilities/stackvec.h"

#include <memory>

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
    std::unique_ptr<uint64_t> storage{new uint64_t[(privateDataSizeNeeded + sizeof(uint64_t) - 1) / sizeof(uint64_t)]};
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

bool canUseAdapterBasedOnDriverDesc(const char *driverDescription) {
    return (strstr(driverDescription, "Intel") != nullptr) ||
           (strstr(driverDescription, "Citrix") != nullptr) ||
           (strstr(driverDescription, "Virtual Render") != nullptr);
}

bool isAllowedDeviceId(uint32_t deviceId) {
    if (DebugManager.flags.ForceDeviceId.get() == "unk") {
        return true;
    }

    char *endptr = nullptr;
    auto reqDeviceId = strtoul(DebugManager.flags.ForceDeviceId.get().c_str(), &endptr, 16);
    return (static_cast<uint32_t>(reqDeviceId) == deviceId);
}

DxCoreAdapterFactory::DxCoreAdapterFactory(DXCoreCreateAdapterFactoryFcn createAdapterFactoryFcn) : createAdapterFactoryFcn(createAdapterFactoryFcn) {
    if (nullptr == createAdapterFactoryFcn) {
        return;
    }

    HRESULT hr = createAdapterFactoryFcn(__uuidof(IDXCoreAdapterFactory), (void **)(&adapterFactory));
    if (hr != S_OK) {
        adapterFactory = nullptr;
    }
}

DxCoreAdapterFactory::~DxCoreAdapterFactory() {
    destroyCurrentSnapshot();

    if (adapterFactory) {
        adapterFactory->Release();
        adapterFactory = nullptr;
    }
}

bool DxCoreAdapterFactory::createSnapshotOfAvailableAdapters() {
    if (false == this->isSupported()) {
        DEBUG_BREAK_IF(true);
        return false;
    }
    destroyCurrentSnapshot();

    GUID attributes[]{DXCORE_ADAPTER_ATTRIBUTE_D3D12_CORE_COMPUTE};
    HRESULT hr = adapterFactory->CreateAdapterList(1, attributes, __uuidof(IDXCoreAdapterList), (void **)(&adaptersInSnapshot));
    if ((hr != S_OK) || (adaptersInSnapshot == nullptr)) {
        DEBUG_BREAK_IF(true);
        destroyCurrentSnapshot();
        return false;
    }

    return true;
}

uint32_t DxCoreAdapterFactory::getNumAdaptersInSnapshot() {
    if (nullptr == adaptersInSnapshot) {
        return 0U;
    }
    return adaptersInSnapshot->GetAdapterCount();
}

bool DxCoreAdapterFactory::getAdapterDesc(uint32_t ordinal, AdapterDesc &outAdapter) {
    if (ordinal >= getNumAdaptersInSnapshot()) {
        DEBUG_BREAK_IF(true);
        return false;
    }

    IDXCoreAdapter *adapter = nullptr;
    HRESULT hr = adaptersInSnapshot->GetAdapter(ordinal, __uuidof(IDXCoreAdapter), (void **)&adapter);
    if ((hr != S_OK) || (adapter == nullptr)) {
        return false;
    }

    outAdapter = {};
    bool isHardware = false;
    hr = adapter->GetProperty(DXCoreAdapterProperty::IsHardware, &isHardware);
    DEBUG_BREAK_IF(S_OK != hr);
    outAdapter.type = isHardware ? AdapterDesc::Type::Hardware : AdapterDesc::Type::NotHardware;

    static constexpr uint32_t maxDriverDescriptionStaticSize = 512;
    StackVec<char, maxDriverDescriptionStaticSize> driverDescription;
    size_t driverDescSize = 0;
    hr = adapter->GetPropertySize(DXCoreAdapterProperty::DriverDescription, &driverDescSize);
    if (S_OK == hr) {
        driverDescription.resize(driverDescSize);
    }
    hr = adapter->GetProperty(DXCoreAdapterProperty::DriverDescription, driverDescription.size(), driverDescription.data());
    if (S_OK != hr) {
        adapter->Release();
        DEBUG_BREAK_IF(true);
        return false;
    }
    outAdapter.driverDescription = driverDescription.data();

    DXCoreHardwareID hwId = {};
    adapter->GetProperty(DXCoreAdapterProperty::HardwareID, sizeof(hwId), &hwId);
    DEBUG_BREAK_IF(S_OK != hr);
    outAdapter.deviceId = hwId.deviceID;

    LUID luid = {};
    hr = adapter->GetProperty(DXCoreAdapterProperty::InstanceLuid, &luid);
    if (S_OK != hr) {
        adapter->Release();
        DEBUG_BREAK_IF(true);
        return false;
    }
    outAdapter.luid = luid;
    adapter->Release();
    return true;
}

void DxCoreAdapterFactory::destroyCurrentSnapshot() {
    if (adaptersInSnapshot) {
        adaptersInSnapshot->Release();
        adaptersInSnapshot = nullptr;
    }
}

DxgiAdapterFactory::DxgiAdapterFactory(CreateDXGIFactoryFcn createAdapterFactoryFcn) : createAdapterFactoryFcn(createAdapterFactoryFcn) {
    if (nullptr == createAdapterFactoryFcn) {
        return;
    }

    HRESULT hr = createAdapterFactoryFcn(__uuidof(IDXGIFactory), (void **)(&adapterFactory));
    if (hr != S_OK) {
        adapterFactory = nullptr;
    }
}

bool DxgiAdapterFactory::getAdapterDesc(uint32_t ordinal, AdapterDesc &outAdapter) {
    if (ordinal >= getNumAdaptersInSnapshot()) {
        DEBUG_BREAK_IF(true);
        return false;
    }

    outAdapter = adaptersInSnapshot[ordinal];
    return true;
}

bool DxgiAdapterFactory::createSnapshotOfAvailableAdapters() {
    if (false == this->isSupported()) {
        DEBUG_BREAK_IF(true);
        return false;
    }
    destroyCurrentSnapshot();

    uint32_t ordinal = 0;
    IDXGIAdapter1 *adapter = nullptr;
    while (adapterFactory->EnumAdapters1(ordinal++, &adapter) != DXGI_ERROR_NOT_FOUND) {
        DXGI_ADAPTER_DESC1 adapterDesc = {{0}};
        HRESULT hr = adapter->GetDesc1(&adapterDesc);
        if (hr != S_OK) {
            adapter->Release();
            DEBUG_BREAK_IF(true);
            continue;
        }

        adaptersInSnapshot.resize(adaptersInSnapshot.size() + 1);
        auto &dstAdapterDesc = *adaptersInSnapshot.rbegin();
        dstAdapterDesc.luid = adapterDesc.AdapterLuid;
        dstAdapterDesc.deviceId = adapterDesc.DeviceId;
        static constexpr auto driverDescMaxChars = sizeof(adapterDesc.Description) / sizeof(adapterDesc.Description[0]);
        dstAdapterDesc.driverDescription.reserve(driverDescMaxChars);
        for (auto wchar : std::wstring(std::wstring(adapterDesc.Description, adapterDesc.Description + driverDescMaxChars).c_str())) {
            dstAdapterDesc.driverDescription.push_back(static_cast<char>(wchar));
        }

        adapter->Release();
    }

    return true;
}

} // namespace NEO
