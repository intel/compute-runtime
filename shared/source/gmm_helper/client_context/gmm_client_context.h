/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/client_context/gmm_client_context_base.h"

namespace NEO {
class GmmClientContext : public GmmClientContextBase {
  public:
    GmmClientContext(OSInterface *osInterface, HardwareInfo *hwInfo);
};
} // namespace NEO
