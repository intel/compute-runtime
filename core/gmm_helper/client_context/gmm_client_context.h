/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/gmm_helper/client_context/gmm_client_context_base.h"

namespace NEO {
class GmmClientContext : public GmmClientContextBase {
  public:
    GmmClientContext(OSInterface *osInterface, HardwareInfo *hwInfo, decltype(&InitializeGmm) initFunc, decltype(&GmmAdapterDestroy) destroyFunc);
};
} // namespace NEO
