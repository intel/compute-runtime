/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/os_context.h"

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
    uint32_t getDrmContextId() const { return drmContextIds[0]; }

  protected:
    unsigned int engineFlag = 0;
    std::vector<uint32_t> drmContextIds;
    Drm &drm;
};
} // namespace NEO
