/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gmm_helper/client_context/gmm_client_context_base.h"

namespace NEO {
GmmClientContextBase::GmmClientContextBase(GMM_CLIENT clientType, GmmExportEntries &gmmEntries) : gmmEntries(gmmEntries) {
    clientContext = gmmEntries.pfnCreateClientContext(clientType);
}
GmmClientContextBase::~GmmClientContextBase() {
    gmmEntries.pfnDeleteClientContext(clientContext);
};

MEMORY_OBJECT_CONTROL_STATE GmmClientContextBase::cachePolicyGetMemoryObject(GMM_RESOURCE_INFO *pResInfo, GMM_RESOURCE_USAGE_TYPE usage) {
    return clientContext->CachePolicyGetMemoryObject(pResInfo, usage);
}

GMM_RESOURCE_INFO *GmmClientContextBase::createResInfoObject(GMM_RESCREATE_PARAMS *pCreateParams) {
    return clientContext->CreateResInfoObject(pCreateParams);
}

GMM_RESOURCE_INFO *GmmClientContextBase::copyResInfoObject(GMM_RESOURCE_INFO *pSrcRes) {
    return clientContext->CopyResInfoObject(pSrcRes);
}

void GmmClientContextBase::destroyResInfoObject(GMM_RESOURCE_INFO *pResInfo) {
    return clientContext->DestroyResInfoObject(pResInfo);
}

GMM_CLIENT_CONTEXT *GmmClientContextBase::getHandle() const {
    return clientContext;
}
} // namespace NEO
