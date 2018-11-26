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
    OsContextImpl(Drm &drm, EngineInstanceT engineType);
    unsigned int getEngineFlag() const { return engineFlag; }

  protected:
    unsigned int engineFlag = 0;
    Drm &drm;
};
} // namespace OCLRT
