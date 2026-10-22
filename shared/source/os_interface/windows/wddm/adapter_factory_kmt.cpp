/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm/adapter_factory_kmt.h"

#include "shared/source/os_interface/windows/gdi_interface.h"

#include <cwchar>
#include <iterator>

namespace NEO {

namespace {

bool queryAdapterInfo(const Gdi &gdi, D3DKMT_HANDLE adapter, KMTQUERYADAPTERINFOTYPE type, void *data, UINT dataSize) {
    D3DKMT_QUERYADAPTERINFO queryAdapterInfo = {};
    queryAdapterInfo.hAdapter = adapter;
    queryAdapterInfo.Type = type;
    queryAdapterInfo.pPrivateDriverData = data;
    queryAdapterInfo.PrivateDriverDataSize = dataSize;
    return STATUS_SUCCESS == gdi.queryAdapterInfo(&queryAdapterInfo);
}

bool queryAdapterDesc(const Gdi &gdi, D3DKMT_HANDLE adapter, AdapterFactory::AdapterDesc &outAdapter) {
    D3DKMT_ADAPTERTYPE adapterType = {};
    D3DKMT_QUERY_DEVICE_IDS deviceIds = {};
    auto driverDescription = std::make_unique<D3DKMT_DRIVER_DESCRIPTION>();
    if (!queryAdapterInfo(gdi, adapter, KMTQAITYPE_ADAPTERTYPE, &adapterType, sizeof(adapterType)) ||
        !queryAdapterInfo(gdi, adapter, KMTQAITYPE_PHYSICALADAPTERDEVICEIDS, &deviceIds, sizeof(deviceIds)) ||
        !queryAdapterInfo(gdi, adapter, KMTQAITYPE_DRIVER_DESCRIPTION, driverDescription.get(), sizeof(D3DKMT_DRIVER_DESCRIPTION))) {
        return false;
    }

    outAdapter.type = adapterType.SoftwareDevice ? AdapterFactory::AdapterDesc::Type::notHardware : AdapterFactory::AdapterDesc::Type::hardware;
    outAdapter.deviceId = deviceIds.DeviceIds.DeviceID;

    const auto &description = driverDescription->DriverDescription;
    outAdapter.driverDescription.assign(description, description + wcsnlen(description, std::size(description)));
    return true;
}

} // namespace

bool KmtAdapterFactory::createSnapshotOfAvailableAdapters() {
    this->adaptersInSnapshot.clear();

    D3DKMT_ENUMADAPTERS3 enumAdapters = {};
    enumAdapters.Filter.IncludeComputeOnly = 1;
    if (STATUS_SUCCESS != this->gdi.enumAdapters3(&enumAdapters)) {
        return false;
    }

    std::vector<D3DKMT_ADAPTERINFO> adapters(enumAdapters.NumAdapters);
    enumAdapters.pAdapters = adapters.data();
    if (STATUS_SUCCESS != this->gdi.enumAdapters3(&enumAdapters)) {
        return false;
    }
    adapters.resize(enumAdapters.NumAdapters);

    for (const auto &adapter : adapters) {
        AdapterDesc adapterDesc;
        adapterDesc.luid = adapter.AdapterLuid;
        if (queryAdapterDesc(this->gdi, adapter.hAdapter, adapterDesc)) {
            this->adaptersInSnapshot.push_back(std::move(adapterDesc));
        }

        D3DKMT_CLOSEADAPTER closeAdapter = {};
        closeAdapter.hAdapter = adapter.hAdapter;
        this->gdi.closeAdapter(&closeAdapter);
    }
    return true;
}

bool KmtAdapterFactory::isSupported() {
    return nullptr != this->gdi.enumAdapters3;
}

uint32_t KmtAdapterFactory::getNumAdaptersInSnapshot() {
    return static_cast<uint32_t>(this->adaptersInSnapshot.size());
}

bool KmtAdapterFactory::getAdapterDesc(uint32_t ordinal, AdapterDesc &outAdapter) {
    if (ordinal >= this->adaptersInSnapshot.size()) {
        return false;
    }
    outAdapter = this->adaptersInSnapshot[ordinal];
    return true;
}

} // namespace NEO
