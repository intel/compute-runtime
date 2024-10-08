/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {
template <typename Family>
size_t EncodeDispatchKernel<Family>::getDefaultIOHAlignment() {
    size_t alignment = Family::cacheLineSize;
    if (NEO::debugManager.flags.ForceIOHAlignment.get() != -1) {
        alignment = static_cast<size_t>(debugManager.flags.ForceIOHAlignment.get());
    }
    return alignment;
}

template <typename Family>
uint32_t EncodeDispatchKernel<Family>::getThreadCountPerSubslice(const HardwareInfo &hwInfo) {
    return hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.SubSliceCount;
}

} // namespace NEO
