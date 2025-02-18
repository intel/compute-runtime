/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/aub/aub_subcapture.h"
#include "shared/source/command_stream/aub_command_stream_receiver_hw.h"
#include "shared/source/execution_environment/execution_environment.h"

namespace NEO {

template <typename GfxFamily>
class UltAubCommandStreamReceiver : public AUBCommandStreamReceiverHw<GfxFamily> {
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

    TaskCountType flushBcsTask(const BlitPropertiesContainer &blitPropertiesContainer, bool blocking, Device &device) override {
        blitBufferCalled++;
        return BaseClass::flushBcsTask(blitPropertiesContainer, blocking, device);
    }

    void pollForCompletion(bool skipTaskCountCheck) override {
        pollForCompletionCalled++;
        BaseClass::pollForCompletion(skipTaskCountCheck);
    }

    uint32_t blitBufferCalled = 0;
    uint32_t pollForCompletionCalled = 0;
};
} // namespace NEO
