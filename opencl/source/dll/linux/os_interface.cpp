/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/os_interface.h"

#include "shared/source/gmm_helper/gmm_interface.h"
#include "shared/source/os_interface/linux/drm_neo.h"

namespace NEO {

bool OSInterface::osEnableLocalMemory = true;

void OSInterface::setGmmInputArgs(void *args) {
    auto gmmInArgs = reinterpret_cast<GMM_INIT_IN_ARGS *>(args);
    auto adapterBDF = this->get()->getDrm()->getAdapterBDF();
    gmmInArgs->FileDescriptor = adapterBDF.Data;
    gmmInArgs->ClientType = GMM_CLIENT::GMM_OCL_VISTA;
}

} // namespace NEO
