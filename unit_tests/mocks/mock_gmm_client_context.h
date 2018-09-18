/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "unit_tests/mocks/mock_gmm_client_context_base.h"

namespace OCLRT {
class MockGmmClientContext : public MockGmmClientContextBase {
  public:
    MockGmmClientContext(GMM_CLIENT clientType, GmmExportEntries &gmmExportEntries);
};
} // namespace OCLRT
