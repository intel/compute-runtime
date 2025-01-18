/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/external_semaphore.h"

namespace NEO {

std::unique_ptr<ExternalSemaphore> ExternalSemaphore::create(OSInterface *osInterface, ExternalSemaphore::Type type, void *handle, int fd) {
    return nullptr;
}

} // namespace NEO