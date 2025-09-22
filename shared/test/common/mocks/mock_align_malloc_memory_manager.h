/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/mocks/mock_memory_manager.h"

using namespace NEO;

class MockAlignMallocMemoryManager : public MockMemoryManager {
  public:
    MockAlignMallocMemoryManager(ExecutionEnvironment &executionEnvironment, bool enableLocalMemory = false) : MockMemoryManager(enableLocalMemory, executionEnvironment) {
        testMallocRestrictions.minAddress = 0;
        alignMallocRestrictions = nullptr;
        alignMallocCount = 0;
        alignMallocMaxIter = 3;
        returnNullBad = false;
        returnNullGood = false;
    }

    AlignedMallocRestrictions testMallocRestrictions;
    AlignedMallocRestrictions *alignMallocRestrictions;

    static const uintptr_t alignMallocMinAddress = 0x100000;
    static const uintptr_t alignMallocStep = 10;
    int alignMallocMaxIter;
    int alignMallocCount;
    bool returnNullBad;
    bool returnNullGood;

    void *alignedMallocWrapper(size_t size, size_t align) override {
        if (alignMallocCount < alignMallocMaxIter) {
            alignMallocCount++;
            if (!returnNullBad) {
                return reinterpret_cast<void *>(alignMallocMinAddress - alignMallocStep);
            } else {
                return nullptr;
            }
        }
        alignMallocCount = 0;
        if (!returnNullGood) {
            return reinterpret_cast<void *>(alignMallocMinAddress + alignMallocStep);
        } else {
            return nullptr;
        }
    };

    void alignedFreeWrapper(void *) override {
        alignMallocCount = 0;
    }

    AlignedMallocRestrictions *getAlignedMallocRestrictions() override {
        return alignMallocRestrictions;
    }
};