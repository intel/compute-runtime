/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/deferred_deleter.h"

namespace NEO {
std::unique_ptr<DeferredDeleter> createDeferredDeleter() {
    return std::make_unique<DeferredDeleter>();
}
} // namespace NEO
