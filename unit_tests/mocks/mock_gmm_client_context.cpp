/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_gmm_client_context.h"

namespace NEO {
MockGmmClientContext::MockGmmClientContext(GMM_CLIENT clientType, GmmExportEntries &gmmExportEntries) : MockGmmClientContextBase(clientType, gmmExportEntries) {
}
} // namespace NEO
