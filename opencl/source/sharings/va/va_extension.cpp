/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifdef LIBVA

#include "opencl/source/sharings/va/enable_va.h"

#include <memory>

namespace NEO {

void *VaSharingBuilderFactory::getExtensionFunctionAddressExtra(const std::string &functionName) {
    return nullptr;
}

} // namespace NEO
#endif