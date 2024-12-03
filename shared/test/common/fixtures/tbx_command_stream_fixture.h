/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"

namespace NEO {

class CommandStreamReceiver;
class MockDevice;
class MemoryManager;

class TbxCommandStreamFixture {
  public:
    void setUp(MockDevice *pDevice);
    void tearDown();

    CommandStreamReceiver *pCommandStreamReceiver = nullptr;

    MemoryManager *memoryManager = nullptr;
};
} // namespace NEO
