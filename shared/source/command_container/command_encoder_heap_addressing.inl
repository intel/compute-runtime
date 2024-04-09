/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"

namespace NEO {
template <typename Family>
template <bool heaplessModeEnabled>
void EncodeDispatchKernel<Family>::programInlineDataHeapless(uint8_t *inlineDataPtr, EncodeDispatchKernelArgs &args, CommandContainer &container, uint64_t offsetThreadData) {
}

} // namespace NEO
