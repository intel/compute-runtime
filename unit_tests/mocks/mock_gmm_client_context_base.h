/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "gmm_client_context.h"

namespace OCLRT {
class MockGmmClientContextBase : public GmmClientContext {
  public:
    MEMORY_OBJECT_CONTROL_STATE cachePolicyGetMemoryObject(GMM_RESOURCE_INFO *pResInfo, GMM_RESOURCE_USAGE_TYPE usage) override;
    GMM_RESOURCE_INFO *createResInfoObject(GMM_RESCREATE_PARAMS *pCreateParams) override;
    GMM_RESOURCE_INFO *copyResInfoObject(GMM_RESOURCE_INFO *pSrcRes) override;
    void destroyResInfoObject(GMM_RESOURCE_INFO *pResInfo) override;

  protected:
    MockGmmClientContextBase(GMM_CLIENT clientType, GmmExportEntries &gmmExportEntries);
};
} // namespace OCLRT
