/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
bool MemorySynchronizationCommands<Family>::isBarrierWaRequired(const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (debugManager.flags.DisablePipeControlPrecedingPostSyncCommand.get() == 1) {
        return true;
    }
    return false;
}
