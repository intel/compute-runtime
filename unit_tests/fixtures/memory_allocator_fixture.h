/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "unit_tests/fixtures/memory_management_fixture.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"

using namespace OCLRT;

class MemoryAllocatorFixture : public MemoryManagementFixture {
  protected:
    MemoryManager *memoryManager;

  public:
    void SetUp() override {
        MemoryManagementFixture::SetUp();
        memoryManager = new OsAgnosticMemoryManager(false, false);
    }

    void TearDown() override {
        delete memoryManager;
        MemoryManagementFixture::TearDown();
    }
};
