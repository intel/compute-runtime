/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

// clang-format off
#include <initguid.h>
#include <dxcore.h>
#include <dxgi.h>
// clang-format on

#include <string>
#include <memory>
#include <vector>

typedef unsigned int D3DKMT_HANDLE;

namespace NEO {

class Gdi;

std::wstring queryAdapterDriverStorePath(const Gdi &gdi, D3DKMT_HANDLE adapter);

bool canUseAdapterBasedOnDriverDesc(const char *driverDescription);

bool isAllowedDeviceId(uint32_t deviceId);

class AdapterFactory {
  public:
    struct AdapterDesc {
        enum class Type {
            Unknown,
            Hardware,
            NotHardware
        };

        Type type = Type::Unknown;
        std::string driverDescription;
        uint32_t deviceId = {};
        LUID luid = {};
    };

    virtual ~AdapterFactory() = default;

    virtual bool createSnapshotOfAvailableAdapters() = 0;
    virtual uint32_t getNumAdaptersInSnapshot() = 0;
    virtual bool getAdapterDesc(uint32_t ordinal, AdapterDesc &outAdapter) = 0;
    virtual bool isSupported() = 0;
};

class DxCoreAdapterFactory : public AdapterFactory {
  public:
    using DXCoreCreateAdapterFactoryFcn = HRESULT(WINAPI *)(REFIID riid, void **ppFactory);
    DxCoreAdapterFactory(DXCoreCreateAdapterFactoryFcn createAdapterFactoryFcn);

    ~DxCoreAdapterFactory() override;

    bool createSnapshotOfAvailableAdapters() override;

    bool isSupported() {
        return nullptr != adapterFactory;
    }

    uint32_t getNumAdaptersInSnapshot() override;

    bool getAdapterDesc(uint32_t ordinal, AdapterDesc &outAdapter) override;

  protected:
    void destroyCurrentSnapshot();

    DXCoreCreateAdapterFactoryFcn createAdapterFactoryFcn = nullptr;
    IDXCoreAdapterFactory *adapterFactory = nullptr;
    IDXCoreAdapterList *adaptersInSnapshot = nullptr;
};

class DxgiAdapterFactory : public AdapterFactory {
  public:
    using CreateDXGIFactoryFcn = HRESULT(WINAPI *)(REFIID riid, void **ppFactory);
    DxgiAdapterFactory(CreateDXGIFactoryFcn createAdapterFactoryFcn);

    ~DxgiAdapterFactory() override {
        destroyCurrentSnapshot();
        if (adapterFactory) {
            adapterFactory->Release();
        }
    }

    bool createSnapshotOfAvailableAdapters() override;

    bool isSupported() {
        return nullptr != adapterFactory;
    }

    uint32_t getNumAdaptersInSnapshot() {
        return static_cast<uint32_t>(adaptersInSnapshot.size());
    }

    bool getAdapterDesc(uint32_t ordinal, AdapterDesc &outAdapter) override;

  protected:
    void destroyCurrentSnapshot() {
        adaptersInSnapshot.clear();
    }

    CreateDXGIFactoryFcn createAdapterFactoryFcn = nullptr;
    IDXGIFactory1 *adapterFactory = nullptr;
    std::vector<AdapterDesc> adaptersInSnapshot;
};

class WddmAdapterFactory : public AdapterFactory {
  public:
    WddmAdapterFactory(DxCoreAdapterFactory::DXCoreCreateAdapterFactoryFcn dxCoreCreateAdapterFactoryF,
                       DxgiAdapterFactory::CreateDXGIFactoryFcn dxgiCreateAdapterFactoryF) {
        underlyingFactory = std::make_unique<DxCoreAdapterFactory>(dxCoreCreateAdapterFactoryF);
        if (false == underlyingFactory->isSupported()) {
            underlyingFactory = std::make_unique<DxgiAdapterFactory>(dxgiCreateAdapterFactoryF);
        }
    }

    ~WddmAdapterFactory() override = default;

    bool createSnapshotOfAvailableAdapters() override {
        return underlyingFactory->createSnapshotOfAvailableAdapters();
    }

    bool isSupported() override {
        return underlyingFactory->isSupported();
    }

    uint32_t getNumAdaptersInSnapshot() override {
        return underlyingFactory->getNumAdaptersInSnapshot();
    }

    bool getAdapterDesc(uint32_t ordinal, AdapterDesc &outAdapter) override {
        return underlyingFactory->getAdapterDesc(ordinal, outAdapter);
    }

  protected:
    std::unique_ptr<AdapterFactory> underlyingFactory;
};

} // namespace NEO
