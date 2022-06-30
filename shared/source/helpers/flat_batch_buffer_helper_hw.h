/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/flat_batch_buffer_helper.h"

namespace NEO {

template <typename GfxFamily>
class FlatBatchBufferHelperHw : public FlatBatchBufferHelper {
  public:
    using FlatBatchBufferHelper::FlatBatchBufferHelper;
    GraphicsAllocation *flattenBatchBuffer(uint32_t rootDeviceIndex, BatchBuffer &batchBuffer, size_t &sizeBatchBuffer, DispatchMode dispatchMode, DeviceBitfield deviceBitfield) override;
    char *getIndirectPatchCommands(size_t &indirectPatchCommandsSize, std::vector<PatchInfoData> &indirectPatchInfo) override;
    void removePipeControlData(size_t pipeControlLocationSize, void *pipeControlForNooping, const HardwareInfo &hwInfo) override;
    void collectScratchSpacePatchInfo(uint64_t scratchAddress, uint64_t commandOffset, const LinearStream &csr) override;
};

} // namespace NEO
