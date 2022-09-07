/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/bcs_split.h"

#include "shared/source/command_stream/command_stream_receiver.h"

#include "level_zero/core/source/device/device_imp.h"

#include <mutex>

namespace L0 {

void BcsSplit::setupDevice(uint32_t productFamily, bool internalUsage, const ze_command_queue_desc_t *desc, NEO::CommandStreamReceiver *csr) {
    static std::mutex bcsSplitInitMutex;
    std::lock_guard<std::mutex> lock(bcsSplitInitMutex);

    auto initializeBcsSplit = this->device.getNEODevice()->isBcsSplitSupported() &&
                              csr->getOsContext().getEngineType() == aub_stream::EngineType::ENGINE_BCS &&
                              !internalUsage &&
                              this->cmdQs.empty();

    if (!initializeBcsSplit) {
        return;
    }

    if (NEO::DebugManager.flags.SplitBcsMask.get() > 0) {
        this->engines = NEO::DebugManager.flags.SplitBcsMask.get();
    }

    ze_command_queue_desc_t splitDesc;
    memcpy(&splitDesc, desc, sizeof(ze_command_queue_desc_t));
    splitDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    for (uint32_t i = 0; i < NEO::bcsInfoMaskSize; i++) {
        if (this->engines.test(i)) {
            auto engineType = (i == 0u ? aub_stream::EngineType::ENGINE_BCS : aub_stream::EngineType::ENGINE_BCS1 + i - 1);
            auto csr = this->device.getNEODevice()->getNearestGenericSubDevice(0u)->getEngine(static_cast<aub_stream::EngineType>(engineType), NEO::EngineUsage::Regular).commandStreamReceiver;

            ze_result_t result;
            auto commandQueue = CommandQueue::create(productFamily, &device, csr, &splitDesc, true, false, result);
            UNRECOVERABLE_IF(result != ZE_RESULT_SUCCESS);

            this->cmdQs.push_back(commandQueue);
        }
    }
}

void BcsSplit::releaseResources() {
    for (auto cmdQ : cmdQs) {
        cmdQ->destroy();
    }
}
} // namespace L0