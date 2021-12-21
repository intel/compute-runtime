/*
 * Copyright (C) 2018-2021 Intel Corporation
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
    void SetUp(MockDevice *pDevice);
    void TearDown();

    CommandStreamReceiver *pCommandStreamReceiver = nullptr;

    MemoryManager *memoryManager = nullptr;
};
} // namespace NEO
