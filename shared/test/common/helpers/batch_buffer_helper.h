/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/submissions_aggregator.h"

using namespace NEO;

struct BatchBufferHelper {
    static BatchBuffer createDefaultBatchBuffer(GraphicsAllocation *commandBufferAllocation, LinearStream *stream, size_t usedSize, size_t chainedBatchBufferStartOffset) {
        return BatchBuffer(
            commandBufferAllocation,            // commandBufferAllocation
            0,                                  // startOffset
            chainedBatchBufferStartOffset,      // chainedBatchBufferStartOffset
            0,                                  // taskStartAddress
            nullptr,                            // chainedBatchBuffer
            false,                              // lowPriority
            QueueThrottle::MEDIUM,              // throttle
            QueueSliceCount::defaultSliceCount, // sliceCount
            usedSize,                           // usedSize
            stream,                             // stream
            nullptr,                            // endCmdPtr
            0,                                  // numCsrClients
            false,                              // hasStallingCmds
            false,                              // hasRelaxedOrderingDependencies
            false,                              // dispatchMonitorFence
            false                               // taskCountUpdateOnly
        );
    }

    static BatchBuffer createDefaultBatchBuffer(GraphicsAllocation *commandBufferAllocation, LinearStream *stream, size_t usedSize) {
        return createDefaultBatchBuffer(commandBufferAllocation, stream, usedSize, 0);
    }
};
