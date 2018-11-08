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
    OsContextImpl(Drm &drm);

  protected:
    Drm &drm;
};
} // namespace OCLRT
