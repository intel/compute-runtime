/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace NEO {
class MemoryManager;
class CommandStreamReceiver;
class Context;
class MultiGraphicsAllocation;
class MemObj;
class MigrationController {
  public:
    static void handleMigration(Context &context, CommandStreamReceiver &targetCsr, MemObj *memObj);
    static void migrateMemory(Context &context, MemoryManager &memoryManager, MemObj *memObj, uint32_t targetRootDeviceIndex);
};
} // namespace NEO