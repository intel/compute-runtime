/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/utilities/software_tags_manager.h"

namespace L0 {
template <typename GfxFamily>
class SWTagScope : public NEO::NonCopyableAndNonMovableClass {
  public:
    SWTagScope() = delete;

    SWTagScope(NEO::Device &device, NEO::LinearStream &cmdStream, const char *callName,
               NEO::SWTags::CounterContext &counters, NEO::SWTags::CounterType counterType, bool isBcs, bool &scopeActive)
        : device(device), cmdStream(cmdStream), scopeActive(scopeActive), callName(callName) {
        scopeActive = true;
        tagsManager = device.getRootDeviceEnvironment().tagsManager.get();
        callId = tagsManager->incrementAndGetCurrentCallCount();
        tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameBeginTag>(cmdStream, device, callName, callId);

        if (NEO::SWTagsManager::countersEnabled()) {
            tagsManager->insertCounterUpdate<GfxFamily>(cmdStream, counterType, counters.incrementAndGet(counterType), isBcs);
        }
    }

    ~SWTagScope() {
        tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameEndTag>(cmdStream, device, callName, callId);
        scopeActive = false;
    }

  private:
    NEO::Device &device;
    NEO::LinearStream &cmdStream;
    bool &scopeActive;
    const char *callName = nullptr;
    NEO::SWTagsManager *tagsManager = nullptr;
    uint32_t callId = 0;
};

} // namespace L0
