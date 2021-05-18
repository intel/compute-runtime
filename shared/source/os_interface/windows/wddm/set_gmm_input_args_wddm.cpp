/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm/wddm.h"

#include "gmm_client_context.h"

namespace NEO {

void Wddm::setGmmInputArgs(void *args) {
    auto gmmInArgs = reinterpret_cast<GMM_INIT_IN_ARGS *>(args);

    gmmInArgs->stAdapterBDF = this->adapterBDF;
    gmmInArgs->ClientType = GMM_CLIENT::GMM_OCL_VISTA;
    gmmInArgs->DeviceRegistryPath = const_cast<char *>(deviceRegistryPath.c_str());
}

} // namespace NEO
