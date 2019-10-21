/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/gmm_helper/gmm_lib.h"

#include <memory>

namespace NEO {
class GmmClientContext;
class GmmClientContextBase {
  public:
    virtual ~GmmClientContextBase();

    MOCKABLE_VIRTUAL MEMORY_OBJECT_CONTROL_STATE cachePolicyGetMemoryObject(GMM_RESOURCE_INFO *pResInfo, GMM_RESOURCE_USAGE_TYPE usage);
    MOCKABLE_VIRTUAL GMM_RESOURCE_INFO *createResInfoObject(GMM_RESCREATE_PARAMS *pCreateParams);
    MOCKABLE_VIRTUAL GMM_RESOURCE_INFO *copyResInfoObject(GMM_RESOURCE_INFO *pSrcRes);
    MOCKABLE_VIRTUAL void destroyResInfoObject(GMM_RESOURCE_INFO *pResInfo);
    GMM_CLIENT_CONTEXT *getHandle() const;
    template <typename T>
    static std::unique_ptr<GmmClientContext> create(GMM_CLIENT clientType, GmmExportEntries &gmmEntries) {
        return std::make_unique<T>(clientType, gmmEntries);
    }

    MOCKABLE_VIRTUAL uint8_t getSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT format);
    MOCKABLE_VIRTUAL uint8_t getMediaSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT format);

  protected:
    GMM_CLIENT_CONTEXT *clientContext;
    GmmClientContextBase(GMM_CLIENT clientType, GmmExportEntries &gmmEntries);
    GmmExportEntries &gmmEntries;
};
} // namespace NEO
