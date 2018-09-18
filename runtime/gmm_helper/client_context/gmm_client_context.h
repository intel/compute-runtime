/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/gmm_helper/client_context/gmm_client_context_base.h"

namespace OCLRT {
class GmmClientContext : public GmmClientContextBase {
  public:
    GmmClientContext(GMM_CLIENT clientType, GmmExportEntries &gmmEntries);
};
} // namespace OCLRT
