/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"

namespace NEO {

bool OSInterface::osEnabled64kbPages = true;
bool OSInterface::newResourceImplicitFlush = false;
bool OSInterface::gpuIdleImplicitFlush = false;
bool OSInterface::requiresSupportForWddmTrimNotification = true;

bool OSInterface::isDebugAttachAvailable() const {
    return false;
}
} // namespace NEO
