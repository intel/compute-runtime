/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {
template <typename Family>
size_t EncodeDispatchKernel<Family>::getDefaultIOHAlignment() {
    return 1;
}

template <typename Family>
uint32_t EncodeDispatchKernel<Family>::getThreadCountPerSubslice(const HardwareInfo &hwInfo) {
    return hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.DualSubSliceCount;
}

template <typename Family>
uint32_t EncodeDispatchKernel<Family>::alignPreferredSlmSize(uint32_t slmSize) {
    return EncodeDispatchKernel<Family>::alignSlmSize(slmSize);
}

} // namespace NEO
