/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"

namespace NEO {
class MockInternalAllocationStorage : public InternalAllocationStorage {
  public:
    using InternalAllocationStorage::InternalAllocationStorage;
    void cleanAllocationList(uint32_t waitTaskCount, uint32_t allocationUsage) override {
        cleanAllocationsCalled++;
        lastCleanAllocationsTaskCount = waitTaskCount;
        lastCleanAllocationUsage = allocationUsage;
        InternalAllocationStorage::cleanAllocationList(waitTaskCount, allocationUsage);
        if (doUpdateCompletion) {
            *commandStreamReceiver.getTagAddress() = valueToUpdateCompletion;
            doUpdateCompletion = false;
        }
    }
    void updateCompletionAfterCleaningList(uint32_t newValue) {
        doUpdateCompletion = true;
        valueToUpdateCompletion = newValue;
    }
    bool doUpdateCompletion = false;
    uint32_t valueToUpdateCompletion;
    uint32_t lastCleanAllocationUsage = 0;
    uint32_t lastCleanAllocationsTaskCount = 0;
    uint32_t cleanAllocationsCalled = 0;
};
} // namespace NEO
