/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/mocks/mock_gmm_client_context_base.h"

namespace NEO {
class MockGmmClientContext : public MockGmmClientContextBase {
  public:
    MockGmmClientContext(OSInterface *osInterface, HardwareInfo *hwInfo);
};
} // namespace NEO
