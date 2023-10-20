/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/dxgi_wrapper.h"
#include "shared/source/os_interface/windows/wddm/adapter_factory.h"

#include <vector>

namespace NEO {

class DxgiAdapterFactory : public AdapterFactory {
  public:
    DxgiAdapterFactory(AdapterFactory::CreateAdapterFactoryFcn createAdapterFactoryFcn);

    ~DxgiAdapterFactory() override {
        destroyCurrentSnapshot();
        if (adapterFactory) {
            adapterFactory->Release();
        }
    }

    bool createSnapshotOfAvailableAdapters() override;

    bool isSupported() override {
        return nullptr != adapterFactory;
    }

    uint32_t getNumAdaptersInSnapshot() override {
        return static_cast<uint32_t>(adaptersInSnapshot.size());
    }

    bool getAdapterDesc(uint32_t ordinal, AdapterDesc &outAdapter) override;

  protected:
    void destroyCurrentSnapshot() {
        adaptersInSnapshot.clear();
    }

    AdapterFactory::CreateAdapterFactoryFcn createAdapterFactoryFcn = nullptr;
    IDXGIFactory1 *adapterFactory = nullptr;
    std::vector<AdapterDesc> adaptersInSnapshot;
};

} // namespace NEO
