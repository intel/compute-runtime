/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/direct_submission/dispatchers/dispatcher.h"

namespace NEO {

class LinearStream;

template <typename GfxFamily>
inline void Dispatcher<GfxFamily>::dispatchStartCommandBuffer(LinearStream &cmdBuffer, uint64_t gpuStartAddress) {
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;

    MI_BATCH_BUFFER_START cmd = GfxFamily::cmdInitBatchBufferStart;
    cmd.setBatchBufferStartAddressGraphicsaddress472(gpuStartAddress);
    cmd.setAddressSpaceIndicator(MI_BATCH_BUFFER_START::ADDRESS_SPACE_INDICATOR_PPGTT);

    auto bbufferStart = cmdBuffer.getSpaceForCmd<MI_BATCH_BUFFER_START>();
    *bbufferStart = cmd;
}

template <typename GfxFamily>
inline size_t Dispatcher<GfxFamily>::getSizeStartCommandBuffer() {
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;
    return sizeof(MI_BATCH_BUFFER_START);
}

template <typename GfxFamily>
inline void Dispatcher<GfxFamily>::dispatchStopCommandBuffer(LinearStream &cmdBuffer) {
    using MI_BATCH_BUFFER_END = typename GfxFamily::MI_BATCH_BUFFER_END;

    MI_BATCH_BUFFER_END cmd = GfxFamily::cmdInitBatchBufferEnd;

    auto bbufferEnd = cmdBuffer.getSpaceForCmd<MI_BATCH_BUFFER_END>();
    *bbufferEnd = cmd;
}

template <typename GfxFamily>
inline size_t Dispatcher<GfxFamily>::getSizeStopCommandBuffer() {
    using MI_BATCH_BUFFER_END = typename GfxFamily::MI_BATCH_BUFFER_END;
    return sizeof(MI_BATCH_BUFFER_END);
}

template <typename GfxFamily>
inline void Dispatcher<GfxFamily>::dispatchStoreDwordCommand(LinearStream &cmdBuffer, uint64_t gpuVa, uint32_t value) {
    using MI_STORE_DATA_IMM = typename GfxFamily::MI_STORE_DATA_IMM;

    MI_STORE_DATA_IMM cmd = GfxFamily::cmdInitStoreDataImm;
    cmd.setAddress(gpuVa);
    cmd.setDataDword0(value);

    auto storeDataImmediate = cmdBuffer.getSpaceForCmd<MI_STORE_DATA_IMM>();
    *storeDataImmediate = cmd;
}

template <typename GfxFamily>
inline size_t Dispatcher<GfxFamily>::getSizeStoreDwordCommand() {
    using MI_STORE_DATA_IMM = typename GfxFamily::MI_STORE_DATA_IMM;
    return sizeof(MI_STORE_DATA_IMM);
}

} // namespace NEO
