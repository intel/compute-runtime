/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/os_library.h"
#include "shared/source/os_interface/windows/dxcore_wrapper.h"
#include "shared/source/os_interface/windows/wddm/adapter_factory.h"

#include <memory>

namespace NEO {

class DxCoreAdapterFactory : public AdapterFactory {
  public:
    DxCoreAdapterFactory(AdapterFactory::CreateAdapterFactoryFcn createAdapterFactoryFcn);

    ~DxCoreAdapterFactory() override;

    bool createSnapshotOfAvailableAdapters() override;

    bool isSupported() override {
        return nullptr != adapterFactory;
    }

    uint32_t getNumAdaptersInSnapshot() override;

    bool getAdapterDesc(uint32_t ordinal, AdapterDesc &outAdapter) override;

  protected:
    void destroyCurrentSnapshot();

    void loadDxCore(DxCoreAdapterFactory::CreateAdapterFactoryFcn &outDxCoreCreateAdapterFactoryF);
    std::unique_ptr<OsLibrary> dxCoreLibrary;

    AdapterFactory::CreateAdapterFactoryFcn createAdapterFactoryFcn = nullptr;
    IDXCoreAdapterFactory *adapterFactory = nullptr;
    IDXCoreAdapterList *adaptersInSnapshot = nullptr;
};

} // namespace NEO
