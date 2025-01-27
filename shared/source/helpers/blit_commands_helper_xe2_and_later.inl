/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/blit_commands_helper.h"

namespace NEO {

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendBlitMemSetCompressionFormat(void *blitCmd, NEO::GraphicsAllocation *dstAlloc, uint32_t compressionFormat) {
    using MEM_SET = typename Family::MEM_SET;

    auto memSetCmd = reinterpret_cast<MEM_SET *>(blitCmd);

    if (dstAlloc->isCompressionEnabled()) {
        memSetCmd->setCompressionFormat(compressionFormat);
    }

    if (debugManager.flags.EnableStatelessCompressionWithUnifiedMemory.get()) {
        if (!MemoryPoolHelper::isSystemMemoryPool(dstAlloc->getMemoryPool())) {
            memSetCmd->setCompressionFormat(debugManager.flags.FormatForStatelessCompressionWithUnifiedMemory.get());
        }
    }
}

} // namespace NEO
