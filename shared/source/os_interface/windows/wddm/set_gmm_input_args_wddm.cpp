/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {

void Wddm::setGmmInputArgs(void *args) {
    auto gmmInArgs = reinterpret_cast<GMM_INIT_IN_ARGS *>(args);

    gmmInArgs->Platform.eRenderCoreFamily = gfxPlatform->eRenderCoreFamily;
    gmmInArgs->Platform.eDisplayCoreFamily = gfxPlatform->eDisplayCoreFamily;
    gmmInArgs->pSkuTable = gfxFeatureTable.get();
    gmmInArgs->pWaTable = gfxWorkaroundTable.get();

    gmmInArgs->stAdapterBDF.Data = this->adapterBDF.data;
    gmmInArgs->ClientType = GMM_CLIENT::GMM_OCL_VISTA;
    gmmInArgs->DeviceRegistryPath = deviceRegistryPath.c_str();
}

} // namespace NEO
