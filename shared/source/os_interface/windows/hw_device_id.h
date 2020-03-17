/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"

#include <d3dkmthk.h>

#include <memory>

namespace NEO {
class Gdi;
struct OsEnvironment;
class HwDeviceId : NonCopyableClass {
  public:
    HwDeviceId(D3DKMT_HANDLE adapterIn, LUID adapterLuidIn, OsEnvironment *osEnvironmentIn);
    Gdi *getGdi() const;
    constexpr D3DKMT_HANDLE getAdapter() const {
        return adapter;
    }
    constexpr LUID getAdapterLuid() const {
        return adapterLuid;
    }
    ~HwDeviceId();

  protected:
    const D3DKMT_HANDLE adapter;
    const LUID adapterLuid;
    OsEnvironment *osEnvironment;
};
} // namespace NEO
