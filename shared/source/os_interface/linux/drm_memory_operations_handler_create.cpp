/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_memory_operations_handler_default.h"

namespace NEO {

std::unique_ptr<DrmMemoryOperationsHandler> DrmMemoryOperationsHandler::create(Drm &drm) {
    return std::make_unique<DrmMemoryOperationsHandlerDefault>();
}

} // namespace NEO
