/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/windows/wddm/um_km_data_translator.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"

#include <d3dkmthk.h>

#include <memory>

namespace NEO {
class Gdi;
struct OsEnvironment;
class HwDeviceId : NonCopyableClass {
  public:
    HwDeviceId(D3DKMT_HANDLE adapterIn, LUID adapterLuidIn, OsEnvironment *osEnvironmentIn, std::unique_ptr<UmKmDataTranslator> umKmDataTranslator);
    Gdi *getGdi() const;
    constexpr D3DKMT_HANDLE getAdapter() const {
        return adapter;
    }
    constexpr LUID getAdapterLuid() const {
        return adapterLuid;
    }
    ~HwDeviceId();

    UmKmDataTranslator *getUmKmDataTranslator() {
        return umKmDataTranslator.get();
    }

  protected:
    const D3DKMT_HANDLE adapter;
    const LUID adapterLuid;
    OsEnvironment *osEnvironment;
    std::unique_ptr<UmKmDataTranslator> umKmDataTranslator;
};
} // namespace NEO
