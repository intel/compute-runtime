/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifdef LIBVA

#include "level_zero/api/opencl/source/sharings/va/enable_va.h"

#include <memory>

namespace NEO {
namespace LEO {

void *VaSharingBuilderFactory::getExtensionFunctionAddressExtra(const std::string &functionName) {
    return nullptr;
}

} // namespace LEO
} // namespace NEO
#endif
