/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "gmm_client_context.h"

namespace OCLRT {
GmmClientContext::GmmClientContext(GMM_CLIENT clientType, GmmExportEntries &gmmEntries) : GmmClientContextBase(clientType, gmmEntries){};
} // namespace OCLRT
