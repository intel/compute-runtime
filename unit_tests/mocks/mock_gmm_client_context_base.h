/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "gmm_client_context.h"

namespace NEO {
class MockGmmClientContextBase : public GmmClientContext {
  public:
    MEMORY_OBJECT_CONTROL_STATE cachePolicyGetMemoryObject(GMM_RESOURCE_INFO *pResInfo, GMM_RESOURCE_USAGE_TYPE usage) override;
    GMM_RESOURCE_INFO *createResInfoObject(GMM_RESCREATE_PARAMS *pCreateParams) override;
    GMM_RESOURCE_INFO *copyResInfoObject(GMM_RESOURCE_INFO *pSrcRes) override;
    void destroyResInfoObject(GMM_RESOURCE_INFO *pResInfo) override;
    uint8_t getSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT format) override;
    uint8_t getMediaSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT format) override;

    GMM_RESOURCE_FORMAT capturedFormat = GMM_FORMAT_INVALID;
    uint8_t compressionFormatToReturn = 1;
    uint32_t getSurfaceStateCompressionFormatCalled = 0u;
    uint32_t getMediaSurfaceStateCompressionFormatCalled = 0u;

  protected:
    using GmmClientContext::GmmClientContext;
};
} // namespace NEO
