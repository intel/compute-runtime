/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/os_context.h"

namespace OCLRT {
class Drm;
using OsContextLinux = OsContext::OsContextImpl;

class OsContext::OsContextImpl {
  public:
    ~OsContextImpl();
    OsContextImpl(Drm &drm, EngineInstanceT engineType);
    unsigned int getEngineFlag() const { return engineFlag; }
    uint32_t getDrmContextId() const { return drmContextId; }

  protected:
    unsigned int engineFlag = 0;
    uint32_t drmContextId = 0;
    Drm &drm;
};
} // namespace OCLRT
