/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {
template <typename GfxFamily>
size_t DebuggerL0Hw<GfxFamily>::getSbaTrackingCommandsSize(size_t trackedAddressCount) {
    if (singleAddressSpaceSbaTracking) {
        UNRECOVERABLE_IF(true);
        return 0;
    }
    return trackedAddressCount * NEO::EncodeStoreMemory<GfxFamily>::getStoreDataImmSize();
}

template <typename GfxFamily>
void DebuggerL0Hw<GfxFamily>::programSbaTrackingCommandsSingleAddressSpace(NEO::LinearStream &cmdStream, const SbaAddresses &sba, bool useFirstLevelBB) {
    UNRECOVERABLE_IF(true);
}
} // namespace NEO