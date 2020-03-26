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
    auto bbufferStart = cmdBuffer.getSpaceForCmd<MI_BATCH_BUFFER_START>();
    *bbufferStart = GfxFamily::cmdInitBatchBufferStart;

    bbufferStart->setBatchBufferStartAddressGraphicsaddress472(gpuStartAddress);
    bbufferStart->setAddressSpaceIndicator(MI_BATCH_BUFFER_START::ADDRESS_SPACE_INDICATOR_PPGTT);
}

template <typename GfxFamily>
inline size_t Dispatcher<GfxFamily>::getSizeStartCommandBuffer() {
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;
    return sizeof(MI_BATCH_BUFFER_START);
}

template <typename GfxFamily>
inline void Dispatcher<GfxFamily>::dispatchStopCommandBuffer(LinearStream &cmdBuffer) {
    using MI_BATCH_BUFFER_END = typename GfxFamily::MI_BATCH_BUFFER_END;

    auto bbufferEnd = cmdBuffer.getSpaceForCmd<MI_BATCH_BUFFER_END>();
    *bbufferEnd = GfxFamily::cmdInitBatchBufferEnd;
}

template <typename GfxFamily>
inline size_t Dispatcher<GfxFamily>::getSizeStopCommandBuffer() {
    using MI_BATCH_BUFFER_END = typename GfxFamily::MI_BATCH_BUFFER_END;
    return sizeof(MI_BATCH_BUFFER_END);
}

template <typename GfxFamily>
inline void Dispatcher<GfxFamily>::dispatchStoreDwordCommand(LinearStream &cmdBuffer, uint64_t gpuVa, uint32_t value) {
    using MI_STORE_DATA_IMM = typename GfxFamily::MI_STORE_DATA_IMM;
    auto storeDataImmediate = cmdBuffer.getSpaceForCmd<MI_STORE_DATA_IMM>();
    *storeDataImmediate = GfxFamily::cmdInitStoreDataImm;

    storeDataImmediate->setAddress(gpuVa);
    storeDataImmediate->setDataDword0(value);
}

template <typename GfxFamily>
inline size_t Dispatcher<GfxFamily>::getSizeStoreDwordCommand() {
    using MI_STORE_DATA_IMM = typename GfxFamily::MI_STORE_DATA_IMM;
    return sizeof(MI_STORE_DATA_IMM);
}

} // namespace NEO
