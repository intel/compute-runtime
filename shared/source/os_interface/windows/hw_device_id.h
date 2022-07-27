/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/wddm/um_km_data_translator.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"

#include <d3dkmthk.h>

#include <memory>

namespace NEO {
class Gdi;
struct OsEnvironment;
class HwDeviceIdWddm : public HwDeviceId {
  public:
    static constexpr DriverModelType driverModelType = DriverModelType::WDDM;

    HwDeviceIdWddm(D3DKMT_HANDLE adapterIn, LUID adapterLuidIn, OsEnvironment *osEnvironmentIn, std::unique_ptr<UmKmDataTranslator> umKmDataTranslator);
    Gdi *getGdi() const;
    constexpr D3DKMT_HANDLE getAdapter() const {
        return adapter;
    }
    constexpr LUID getAdapterLuid() const {
        return adapterLuid;
    }
    ~HwDeviceIdWddm() override;

    UmKmDataTranslator *getUmKmDataTranslator() {
        return umKmDataTranslator.get();
    }

  protected:
    const LUID adapterLuid;
    std::unique_ptr<UmKmDataTranslator> umKmDataTranslator;
    OsEnvironment *osEnvironment;
    const D3DKMT_HANDLE adapter;
};
} // namespace NEO
