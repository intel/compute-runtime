/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/memory_manager/deferrable_deletion.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/windows/d3dkmthk_wrapper.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"

namespace NEO {

class OsContextWin;
class Wddm;
enum class AllocationType;

class DeferrableDeletionImpl : public DeferrableDeletion, NEO::NonCopyableAndNonMovableClass {
  public:
    DeferrableDeletionImpl(Wddm *wddm, const D3DKMT_HANDLE *handles, uint32_t allocationCount, D3DKMT_HANDLE resourceHandle, AllocationType type);
    bool apply() override;
    ~DeferrableDeletionImpl() override;

  protected:
    Wddm *wddm;
    D3DKMT_HANDLE *handles = nullptr;
    uint32_t allocationCount;
    D3DKMT_HANDLE resourceHandle;
};

static_assert(NEO::NonCopyableAndNonMovable<DeferrableDeletionImpl>);

} // namespace NEO
