/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/hw_helper.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/device/device.h"

namespace NEO {

EngineControl &Device::getInternalEngine() {
    if (this->engines[0].commandStreamReceiver->getType() != CommandStreamReceiverType::CSR_HW) {
        return this->getDefaultEngine();
    }
    return this->engines[HwHelper::internalUsageEngineIndex];
}
} // namespace NEO
