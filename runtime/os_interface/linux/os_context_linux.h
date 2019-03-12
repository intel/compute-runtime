/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/os_context.h"

namespace OCLRT {
class Drm;

class OsContextLinux : public OsContext {
  public:
    OsContextLinux() = delete;
    ~OsContextLinux() override;
    OsContextLinux(Drm &drm, uint32_t contextId, uint32_t deviceBitfield,
                   EngineInstanceT engineType, PreemptionMode preemptionMode);

    unsigned int getEngineFlag() const { return engineFlag; }
    uint32_t getDrmContextId() const { return drmContextId; }

  protected:
    unsigned int engineFlag = 0;
    uint32_t drmContextId = 0;
    Drm &drm;
};
} // namespace OCLRT
