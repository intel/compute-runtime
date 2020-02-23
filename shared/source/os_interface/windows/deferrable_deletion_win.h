/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/deferrable_deletion.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"

#include <d3dkmthk.h>

namespace NEO {

class OsContextWin;
class Wddm;

class DeferrableDeletionImpl : public DeferrableDeletion {
  public:
    DeferrableDeletionImpl(Wddm *wddm, const D3DKMT_HANDLE *handles, uint32_t allocationCount, D3DKMT_HANDLE resourceHandle);
    bool apply() override;
    ~DeferrableDeletionImpl();

    DeferrableDeletionImpl(const DeferrableDeletionImpl &) = delete;
    DeferrableDeletionImpl &operator=(const DeferrableDeletionImpl &) = delete;

  protected:
    Wddm *wddm;
    D3DKMT_HANDLE *handles = nullptr;
    uint32_t allocationCount;
    D3DKMT_HANDLE resourceHandle;
};
} // namespace NEO
