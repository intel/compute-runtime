/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

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
