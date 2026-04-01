/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"

namespace NEO {
template <>
bool ProductHelperHw<gfxProduct>::isInterruptSupported(const RootDeviceEnvironment &rootDeviceEnvironment) const {
    return true;
}
} // namespace NEO
