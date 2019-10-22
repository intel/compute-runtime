/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/aub_command_stream_receiver_hw.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "unit_tests/mocks/mock_experimental_command_buffer.h"

namespace NEO {

template <typename GfxFamily>
class UltAubCommandStreamReceiver : public AUBCommandStreamReceiverHw<GfxFamily>, public NonCopyableOrMovableClass {
    using BaseClass = AUBCommandStreamReceiverHw<GfxFamily>;

  public:
    using BaseClass::osContext;

    UltAubCommandStreamReceiver(const std::string &fileName, bool standalone, ExecutionEnvironment &executionEnvironment)
        : BaseClass(fileName, standalone, executionEnvironment) {
    }

    UltAubCommandStreamReceiver(ExecutionEnvironment &executionEnvironment)
        : BaseClass("aubfile", true, executionEnvironment) {
    }

    static CommandStreamReceiver *create(bool withAubDump, ExecutionEnvironment &executionEnvironment) {
        auto csr = new UltAubCommandStreamReceiver<GfxFamily>("aubfile", true, executionEnvironment);

        if (!csr->subCaptureManager->isSubCaptureMode()) {
            csr->openFile("aubfile");
        }

        return csr;
    }

    uint32_t blitBuffer(const BlitProperties &blitProperites) override {
        blitBufferCalled++;
        return BaseClass::blitBuffer(blitProperites);
    }

    uint32_t blitBufferCalled = 0;
};
} // namespace NEO
