/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"

#include <d3dkmthk.h>

#include <memory>

namespace NEO {
class Gdi;
class UmKmDataTranslator;
struct OsEnvironment;

class HwDeviceIdWddm : public HwDeviceId {
  public:
    static constexpr DriverModelType driverModelType = DriverModelType::wddm;

    HwDeviceIdWddm(D3DKMT_HANDLE adapterIn, LUID adapterLuidIn, uint32_t adapterNodeMaskIn, OsEnvironment *osEnvironmentIn, std::unique_ptr<UmKmDataTranslator> umKmDataTranslator);
    Gdi *getGdi() const;
    constexpr D3DKMT_HANDLE getAdapter() const {
        return adapter;
    }
    constexpr LUID getAdapterLuid() const {
        return adapterLuid;
    }
    constexpr uint32_t getAdapterNodeMask() const {
        return adapterNodeMask;
    }
    ~HwDeviceIdWddm() override;

    UmKmDataTranslator *getUmKmDataTranslator() {
        return umKmDataTranslator.get();
    }

  protected:
    const LUID adapterLuid;
    const uint32_t adapterNodeMask;
    std::unique_ptr<UmKmDataTranslator> umKmDataTranslator;
    OsEnvironment *osEnvironment;
    const D3DKMT_HANDLE adapter;
};
} // namespace NEO
