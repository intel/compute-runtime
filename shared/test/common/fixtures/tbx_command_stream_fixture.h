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
    void SetUp(MockDevice *pDevice); // NOLINT(readability-identifier-naming)
    void TearDown();                 // NOLINT(readability-identifier-naming)

    CommandStreamReceiver *pCommandStreamReceiver = nullptr;

    MemoryManager *memoryManager = nullptr;
};
} // namespace NEO
