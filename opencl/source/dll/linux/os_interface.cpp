/*
 * Copyright (C) 2018-2020 Intel Corporation
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
    reinterpret_cast<GMM_INIT_IN_ARGS *>(args)->FileDescriptor = this->get()->getDrm()->getFileDescriptor();
}

} // namespace NEO
