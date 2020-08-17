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
                   aub_stream::EngineType engineType, PreemptionMode preemptionMode,
                   bool lowPriority, bool internalEngine, bool rootDevice);

    unsigned int getEngineFlag() const { return engineFlag; }
    const std::vector<uint32_t> &getDrmContextIds() const { return drmContextIds; }
    const std::vector<uint32_t> &getDrmVmIds() const { return drmVmIds; }
    Drm &getDrm() const;

  protected:
    unsigned int engineFlag = 0;
    std::vector<uint32_t> drmContextIds;
    std::vector<uint32_t> drmVmIds;
    Drm &drm;
};
} // namespace NEO
