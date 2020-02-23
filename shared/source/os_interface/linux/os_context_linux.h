/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/os_context.h"

#include <vector>

namespace NEO {
class Drm;

class OsContextLinux : public OsContext {
  public:
    OsContextLinux() = delete;
    ~OsContextLinux() override;
    OsContextLinux(Drm &drm, uint32_t contextId, DeviceBitfield deviceBitfield,
                   aub_stream::EngineType engineType, PreemptionMode preemptionMode, bool lowPriority);

    unsigned int getEngineFlag() const { return engineFlag; }
    const std::vector<uint32_t> &getDrmContextIds() const { return drmContextIds; }

  protected:
    unsigned int engineFlag = 0;
    std::vector<uint32_t> drmContextIds;
    Drm &drm;
};
} // namespace NEO
