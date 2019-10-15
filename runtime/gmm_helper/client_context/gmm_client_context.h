/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/gmm_helper/client_context/gmm_client_context_base.h"

namespace NEO {
class GmmClientContext : public GmmClientContextBase {
  public:
    GmmClientContext(HardwareInfo *hwInfo, decltype(&InitializeGmm) initFunc, decltype(&GmmDestroy) destroyFunc);
};
} // namespace NEO
