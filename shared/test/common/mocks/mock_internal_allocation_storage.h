/*
 * Copyright (C) 2018-2025 Intel Corporation
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
    using InternalAllocationStorage::allocationLists;
    using InternalAllocationStorage::InternalAllocationStorage;
    void cleanAllocationList(TaskCountType waitTaskCount, uint32_t allocationUsage) override {
        cleanAllocationsCalled++;
        lastCleanAllocationsTaskCount = waitTaskCount;
        lastCleanAllocationUsage = allocationUsage;
        InternalAllocationStorage::cleanAllocationList(waitTaskCount, allocationUsage);
        if (doUpdateCompletion) {
            *commandStreamReceiver.getTagAddress() = valueToUpdateCompletion;
            doUpdateCompletion = false;
        }
    }
    void updateCompletionAfterCleaningList(TaskCountType newValue) {
        doUpdateCompletion = true;
        valueToUpdateCompletion = newValue;
    }
    bool doUpdateCompletion = false;
    TaskCountType valueToUpdateCompletion;
    uint32_t lastCleanAllocationUsage = 0;
    TaskCountType lastCleanAllocationsTaskCount = 0;
    uint32_t cleanAllocationsCalled = 0;
};
} // namespace NEO
