/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/windows_wrapper.h"

#include <cstdint>
#include <memory>
#include <string>

typedef unsigned int D3DKMT_HANDLE;

namespace NEO {

class Gdi;
class OsLibrary;

bool canUseAdapterBasedOnDriverDesc(const char *driverDescription);

bool isAllowedDeviceId(uint32_t deviceId);

class AdapterFactory {
  public:
    using CreateAdapterFactoryFcn = HRESULT(WINAPI *)(REFIID riid, void **ppFactory);

    struct AdapterDesc {
        enum class Type {
            unknown,
            hardware,
            notHardware
        };

        Type type = Type::unknown;
        std::string driverDescription;
        uint32_t deviceId = {};
        LUID luid = {};
    };

    virtual ~AdapterFactory() = default;

    virtual bool createSnapshotOfAvailableAdapters() = 0;
    virtual uint32_t getNumAdaptersInSnapshot() = 0;
    virtual bool getAdapterDesc(uint32_t ordinal, AdapterDesc &outAdapter) = 0;
    virtual bool isSupported() = 0;

    static std::unique_ptr<AdapterFactory> create(AdapterFactory::CreateAdapterFactoryFcn dxCoreCreateAdapterFactoryF,
                                                  AdapterFactory::CreateAdapterFactoryFcn dxgiCreateAdapterFactoryF);
};

} // namespace NEO
