/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/utilities/software_tags_manager.h"

namespace L0 {
template <typename GfxFamily>
class SWTagScope : public NEO::NonCopyableAndNonMovableClass {
  public:
    SWTagScope() = delete;

    SWTagScope(NEO::Device &device, NEO::LinearStream &cmdStream, const char *callName)
        : device(device), cmdStream(cmdStream), callName(callName) {
        tagsManager = device.getRootDeviceEnvironment().tagsManager.get();
        callId = tagsManager->incrementAndGetCurrentCallCount();
        tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameBeginTag>(cmdStream, device, callName, callId);
    }

    ~SWTagScope() {
        tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameEndTag>(cmdStream, device, callName, callId);
    }

  private:
    NEO::Device &device;
    NEO::LinearStream &cmdStream;
    const char *callName = nullptr;
    NEO::SWTagsManager *tagsManager = nullptr;
    uint32_t callId = 0;
};

} // namespace L0