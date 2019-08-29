/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/os_interface/windows/windows_wrapper.h"
#include "runtime/memory_manager/deferrable_deletion.h"
#include "runtime/os_interface/os_context.h"

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
