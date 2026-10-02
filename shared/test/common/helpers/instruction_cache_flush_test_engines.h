/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"

#include <memory>
#include <vector>

namespace NEO {

class InstructionCacheFlushTestEngines {
  public:
    InstructionCacheFlushTestEngines(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex)
        : executionEnvironment(executionEnvironment), rootDeviceIndex(rootDeviceIndex) {
        primary = createEngine(aub_stream::ENGINE_CCS);
        primary->getOsContext().setContextGroupCount(3u);
        secondary = createEngine(aub_stream::ENGINE_CCS, primary);
        sibling = createEngine(aub_stream::ENGINE_CCS, primary);

        unrelatedPrimary = createEngine(aub_stream::ENGINE_CCS);
        unrelatedPrimary->getOsContext().setContextGroupCount(2u);
        unrelatedSecondary = createEngine(aub_stream::ENGINE_CCS, unrelatedPrimary);
    }

    ~InstructionCacheFlushTestEngines() {
        // Destroy secondary contexts before their primary contexts.
        while (!csrs.empty()) {
            csrs.pop_back();
        }
    }

    MockCommandStreamReceiver *createEngine(aub_stream::EngineType engineType,
                                            MockCommandStreamReceiver *primaryCsr = nullptr) {
        auto csr = std::make_unique<MockCommandStreamReceiver>(
            executionEnvironment, rootDeviceIndex, DeviceBitfield{1});

        auto osContext = executionEnvironment.memoryManager->createAndRegisterOsContext(
            csr.get(), EngineDescriptorHelper::getDefaultDescriptor({engineType, EngineUsage::regular}));

        if (primaryCsr != nullptr) {
            osContext->setPrimaryContext(&primaryCsr->getOsContext());
        }

        csr->setupContext(*osContext);

        auto result = csr.get();
        csrs.push_back(std::move(csr));
        return result;
    }

    MockCommandStreamReceiver *primary = nullptr;
    MockCommandStreamReceiver *secondary = nullptr;
    MockCommandStreamReceiver *sibling = nullptr;
    MockCommandStreamReceiver *unrelatedPrimary = nullptr;
    MockCommandStreamReceiver *unrelatedSecondary = nullptr;
    std::vector<std::unique_ptr<MockCommandStreamReceiver>> csrs;

  private:
    ExecutionEnvironment &executionEnvironment;
    uint32_t rootDeviceIndex;
};

} // namespace NEO
