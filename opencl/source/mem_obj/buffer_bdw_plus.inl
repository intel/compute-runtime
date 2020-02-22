/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/mem_obj/buffer_base.inl"

namespace NEO {

template <typename GfxFamily>
void BufferHw<GfxFamily>::appendBufferState(void *memory, Context *context, GraphicsAllocation *gfxAllocation, bool isReadOnly) {
}

} // namespace NEO
