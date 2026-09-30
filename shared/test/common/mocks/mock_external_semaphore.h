/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/external_semaphore.h"

namespace NEO {

class MockExternalSemaphore : public ExternalSemaphore {
  public:
    ImportResult importSemaphore(void *extHandle, int fd, uint32_t flags, const char *name, Type type, bool isNative) override { return ImportResult::success; }
};

} // namespace NEO
