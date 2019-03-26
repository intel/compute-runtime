/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/helpers/flat_batch_buffer_helper.h"

namespace NEO {

template <typename GfxFamily>
class FlatBatchBufferHelperHw : public FlatBatchBufferHelper {
  public:
    using FlatBatchBufferHelper::FlatBatchBufferHelper;
    GraphicsAllocation *flattenBatchBuffer(BatchBuffer &batchBuffer, size_t &sizeBatchBuffer, DispatchMode dispatchMode) override;
    char *getIndirectPatchCommands(size_t &indirectPatchCommandsSize, std::vector<PatchInfoData> &indirectPatchInfo) override;
    void removePipeControlData(size_t pipeControlLocationSize, void *pipeControlForNooping) override;
};

} // namespace NEO
