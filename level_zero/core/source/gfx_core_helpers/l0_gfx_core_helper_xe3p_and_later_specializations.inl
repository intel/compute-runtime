/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
uint32_t L0GfxCoreHelperHw<Family>::getGrfRegisterCount(uint32_t *regPtr) const {
    return (regPtr[4] & 0x3FF);
}

template <>
bool L0GfxCoreHelperHw<Family>::platformSupportsStateBaseAddressTracking(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) const {
    if (rootDeviceEnvironment.getHardwareInfo()->capabilityTable.supportsImages) {
        return false;
    } else {
        return true;
    }
}

template <>
bool L0GfxCoreHelperHw<Family>::implicitSynchronizedDispatchForCooperativeKernelsAllowed() const {
    return true;
}

template <>
bool L0GfxCoreHelperHw<Family>::alwaysAllocateEventInLocalMem() const {
    return true;
}

template <>
bool L0GfxCoreHelperHw<Family>::threadResumeRequiresUnlock() const {
    return true;
}

template <>
bool L0GfxCoreHelperHw<Family>::isThreadControlStoppedSupported() const {
    return false;
}

template <>
bool L0GfxCoreHelperHw<Family>::isCopyOffloadForOutOfOrderImmediateCmdListSupported() const {
    return true;
}
