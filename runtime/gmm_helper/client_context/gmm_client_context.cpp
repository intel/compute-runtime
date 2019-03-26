/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "gmm_client_context.h"

namespace NEO {
GmmClientContext::GmmClientContext(GMM_CLIENT clientType, GmmExportEntries &gmmEntries) : GmmClientContextBase(clientType, gmmEntries){};
} // namespace NEO
