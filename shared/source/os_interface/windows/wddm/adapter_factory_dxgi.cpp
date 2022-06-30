/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm/adapter_factory_dxgi.h"

#include "shared/source/helpers/debug_helpers.h"

#include <memory>

namespace NEO {
DxgiAdapterFactory::DxgiAdapterFactory(AdapterFactory::CreateAdapterFactoryFcn createAdapterFactoryFcn) : createAdapterFactoryFcn(createAdapterFactoryFcn) {
    if (nullptr == createAdapterFactoryFcn) {
        return;
    }

    HRESULT hr = createAdapterFactoryFcn(__uuidof(adapterFactory), (void **)(&adapterFactory));
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
