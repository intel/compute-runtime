/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/client_context/gmm_handle_allocator.h"
#include "shared/source/gmm_helper/gmm_interface.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/sku_info/operations/sku_info_transfer.h"

namespace NEO {
GmmClientContext::GmmClientContext(const RootDeviceEnvironment &rootDeviceEnvironment) {
    auto hardwareInfo = rootDeviceEnvironment.getHardwareInfo();
    _SKU_FEATURE_TABLE gmmFtrTable = {};
    _WA_TABLE gmmWaTable = {};
    SkuInfoTransfer::transferFtrTableForGmm(&gmmFtrTable, &hardwareInfo->featureTable);
    SkuInfoTransfer::transferWaTableForGmm(&gmmWaTable, &hardwareInfo->workaroundTable);

    GMM_INIT_IN_ARGS inArgs{};
    GMM_INIT_OUT_ARGS outArgs{};

    const auto &gtSystemInfo = hardwareInfo->gtSystemInfo;
    inArgs.ClientType = GMM_CLIENT::GMM_OCL_VISTA;
    inArgs.pGtSysInfo = const_cast<GT_SYSTEM_INFO *>(&gtSystemInfo);
    inArgs.pSkuTable = &gmmFtrTable;
    inArgs.pWaTable = &gmmWaTable;
    inArgs.Platform = hardwareInfo->platform;

    auto osInterface = rootDeviceEnvironment.osInterface.get();
    if (osInterface && osInterface->getDriverModel()) {
        osInterface->getDriverModel()->setGmmInputArgs(&inArgs);
    }
    if (debugManager.flags.EnableFtrTile64Optimization.get() != -1) {
        reinterpret_cast<_SKU_FEATURE_TABLE *>(inArgs.pSkuTable)->FtrTile64Optimization = debugManager.flags.EnableFtrTile64Optimization.get();
    }

    auto ret = GmmInterface::initialize(&inArgs, &outArgs);

    UNRECOVERABLE_IF(ret != GMM_SUCCESS);

    clientContext = outArgs.pGmmClientContext;
}
GmmClientContext::~GmmClientContext() {
    GMM_INIT_OUT_ARGS outArgs;
    outArgs.pGmmClientContext = clientContext;

    GmmInterface::destroy(&outArgs);
};

MEMORY_OBJECT_CONTROL_STATE GmmClientContext::cachePolicyGetMemoryObject(GMM_RESOURCE_INFO *pResInfo, GMM_RESOURCE_USAGE_TYPE usage) {
    return clientContext->CachePolicyGetMemoryObject(pResInfo, usage);
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
    return clientContext;
}

uint8_t GmmClientContext::getSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT format) {
    return clientContext->GetSurfaceStateCompressionFormat(format);
}

uint8_t GmmClientContext::getMediaSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT format) {
    return clientContext->GetMediaSurfaceStateCompressionFormat(format);
}

void GmmClientContext::setGmmDeviceInfo(GMM_DEVICE_INFO *deviceInfo) {
    clientContext->GmmSetDeviceInfo(deviceInfo);
}

uint32_t GmmClientContext::cachePolicyGetPATIndex(GMM_RESOURCE_INFO *gmmResourceInfo, GMM_RESOURCE_USAGE_TYPE usage, bool compressed, bool cacheable) {
    bool outValue = compressed;
    uint32_t patIndex = clientContext->CachePolicyGetPATIndex(gmmResourceInfo, usage, &outValue, cacheable);

    DEBUG_BREAK_IF(outValue != compressed);

    return patIndex;
}
void GmmClientContext::setHandleAllocator(std::unique_ptr<GmmHandleAllocator> allocator) {
    this->handleAllocator = std::move(allocator);
}

} // namespace NEO
