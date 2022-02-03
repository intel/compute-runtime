/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/aub_command_stream_receiver_hw.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/test/common/mocks/mock_experimental_command_buffer.h"

namespace NEO {

template <typename GfxFamily>
class UltAubCommandStreamReceiver : public AUBCommandStreamReceiverHw<GfxFamily>, public NonCopyableOrMovableClass {
    using BaseClass = AUBCommandStreamReceiverHw<GfxFamily>;

  public:
    using BaseClass::osContext;
    using BaseClass::useGpuIdleImplicitFlush;
    using BaseClass::useNewResourceImplicitFlush;

    UltAubCommandStreamReceiver(const std::string &fileName,
                                bool standalone,
                                ExecutionEnvironment &executionEnvironment,
                                uint32_t rootDeviceIndex,
                                const DeviceBitfield deviceBitfield)
        : BaseClass(fileName, standalone, executionEnvironment, rootDeviceIndex, deviceBitfield) {
    }

    UltAubCommandStreamReceiver(ExecutionEnvironment &executionEnvironment,
                                uint32_t rootDeviceIndex,
                                const DeviceBitfield deviceBitfield)
        : BaseClass("aubfile", true, executionEnvironment, rootDeviceIndex, deviceBitfield) {
    }

    static CommandStreamReceiver *create(bool withAubDump,
                                         ExecutionEnvironment &executionEnvironment,
                                         uint32_t rootDeviceIndex,
                                         const DeviceBitfield deviceBitfield) {
        auto csr = new UltAubCommandStreamReceiver<GfxFamily>("aubfile", true, executionEnvironment, rootDeviceIndex, deviceBitfield);

        if (!csr->subCaptureManager->isSubCaptureMode()) {
            csr->openFile("aubfile");
        }

        return csr;
    }

    uint32_t flushBcsTask(const BlitPropertiesContainer &blitPropertiesContainer, bool blocking, bool profilingEnabled, Device &device) override {
        blitBufferCalled++;
        return BaseClass::flushBcsTask(blitPropertiesContainer, blocking, profilingEnabled, device);
    }

    void pollForCompletion() override {
        pollForCompletionCalled++;
        BaseClass::pollForCompletion();
    }

    uint32_t blitBufferCalled = 0;
    uint32_t pollForCompletionCalled = 0;
};
} // namespace NEO
