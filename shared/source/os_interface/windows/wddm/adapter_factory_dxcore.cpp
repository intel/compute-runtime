/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#define INITGUID
#include "shared/source/os_interface/windows/wddm/adapter_factory_dxcore.h"

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/os_library.h"
#include "shared/source/os_interface/windows/os_inc.h"
#include "shared/source/utilities/stackvec.h"

#include <memory>

namespace NEO {

DxCoreAdapterFactory::DxCoreAdapterFactory(AdapterFactory::CreateAdapterFactoryFcn createAdapterFactoryFcn) : createAdapterFactoryFcn(createAdapterFactoryFcn) {
    if (nullptr == createAdapterFactoryFcn) {
        dxCoreLibrary.reset(OsLibrary::loadFunc({Os::dxcoreDllName}));
        if (dxCoreLibrary && dxCoreLibrary->isLoaded()) {
            auto func = dxCoreLibrary->getProcAddress(dXCoreCreateAdapterFactoryFuncName);
            createAdapterFactoryFcn = reinterpret_cast<DxCoreAdapterFactory::CreateAdapterFactoryFcn>(func);
        }
        if (nullptr == createAdapterFactoryFcn) {
            return;
        }
    }

    HRESULT hr = createAdapterFactoryFcn(__uuidof(adapterFactory), (void **)(&adapterFactory));
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
    HRESULT hr = adapterFactory->CreateAdapterList(1, attributes, __uuidof(adaptersInSnapshot), (void **)(&adaptersInSnapshot));
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
    [[maybe_unused]] HRESULT hr = adaptersInSnapshot->GetAdapter(ordinal, __uuidof(adapter), (void **)&adapter);
    if ((hr != S_OK) || (adapter == nullptr)) {
        return false;
    }

    outAdapter = {};
    bool isHardware = false;
    hr = adapter->GetProperty(DXCoreAdapterProperty::IsHardware, &isHardware);
    DEBUG_BREAK_IF(S_OK != hr);
    outAdapter.type = isHardware ? AdapterDesc::Type::hardware : AdapterDesc::Type::notHardware;

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
    hr = adapter->GetProperty(DXCoreAdapterProperty::HardwareID, sizeof(hwId), &hwId);
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

} // namespace NEO
