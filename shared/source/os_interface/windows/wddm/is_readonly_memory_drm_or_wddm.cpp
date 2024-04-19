/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {
bool Wddm::isReadOnlyMemory(const void *ptr) {
    return false;
}
} // namespace NEO
