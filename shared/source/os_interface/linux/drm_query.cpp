/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"

namespace NEO {

bool Drm::hasPageFaultSupport() const {
    return false;
}

} // namespace NEO
