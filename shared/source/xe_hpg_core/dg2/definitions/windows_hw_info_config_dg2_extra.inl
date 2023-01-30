/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {
template <>
bool ProductHelperHw<gfxProduct>::isUnlockingLockedPtrNecessary(const HardwareInfo &hwInfo) const {
    return true;
}
} // namespace NEO