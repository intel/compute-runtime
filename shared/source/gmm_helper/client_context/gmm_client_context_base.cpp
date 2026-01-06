/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/client_context/gmm_handle_allocator.h"
#include "shared/source/helpers/debug_helpers.h"

namespace NEO {
GmmClientContext::GmmClientContext() = default;
GmmClientContext::~GmmClientContext() = default;

MEMORY_OBJECT_CONTROL_STATE GmmClientContext::cachePolicyGetMemoryObject(GMM_RESOURCE_INFO *pResInfo, GmmResourceUsageType usage) {
    return clientContext->CachePolicyGetMemoryObject(pResInfo, static_cast<GMM_RESOURCE_USAGE_TYPE_ENUM>(usage));
}

GMM_RESOURCE_INFO *GmmClientContext::createResInfoObject(GMM_RESCREATE_PARAMS *pCreateParams) {
    return clientContext->CreateResInfoObject(pCreateParams);
}

GMM_RESOURCE_INFO *GmmClientContext::copyResInfoObject(GMM_RESOURCE_INFO *pSrcRes) {
    return clientContext->CopyResInfoObject(pSrcRes);
}

void GmmClientContext::destroyResInfoObject(GMM_RESOURCE_INFO *pResInfo) {
    clientContext->DestroyResInfoObject(pResInfo);
}

GMM_CLIENT_CONTEXT *GmmClientContext::getHandle() const {
    return clientContext.get();
}

uint8_t GmmClientContext::getSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT format) {
    return clientContext->GetSurfaceStateCompressionFormat(format);
}

uint8_t GmmClientContext::getMediaSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT format) {
    return clientContext->GetMediaSurfaceStateCompressionFormat(format);
}

uint32_t GmmClientContext::cachePolicyGetPATIndex(GMM_RESOURCE_INFO *gmmResourceInfo, GmmResourceUsageType usage, bool compressed, bool cacheable) {
    bool outValue = compressed;
    uint32_t patIndex = clientContext->CachePolicyGetPATIndex(gmmResourceInfo, static_cast<GMM_RESOURCE_USAGE_TYPE_ENUM>(usage), &outValue, cacheable);

    DEBUG_BREAK_IF(outValue != compressed);

    return patIndex;
}
void GmmClientContext::setHandleAllocator(std::unique_ptr<GmmHandleAllocator> allocator) {
    this->handleAllocator = std::move(allocator);
}

} // namespace NEO
