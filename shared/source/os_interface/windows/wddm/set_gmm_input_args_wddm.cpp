/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {

void Wddm::setGmmInputArgs(void *args) {
    auto gmmInArgs = reinterpret_cast<GMM_INIT_IN_ARGS *>(args);

    gmmInArgs->stAdapterBDF = this->adapterBDF;
    gmmInArgs->ClientType = GMM_CLIENT::GMM_OCL_VISTA;
    gmmInArgs->DeviceRegistryPath = deviceRegistryPath.c_str();
}

} // namespace NEO
