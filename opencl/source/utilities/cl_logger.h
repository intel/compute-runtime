/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/utilities/logger.h"

#include <cinttypes>
#include <cstddef>
#include <string>

namespace NEO {
struct MultiDispatchInfo;

template <DebugFunctionalityLevel debugLevel>
class ClFileLogger : public NonCopyableAndNonMovableClass {
  public:
    ClFileLogger(FileLogger<debugLevel> &baseLoggerInm, const DebugVariables &flags);

    void dumpKernelArgs(const MultiDispatchInfo *multiDispatchInfo);
    const std::string getEvents(const uintptr_t *input, uint32_t numOfEvents);
    const std::string getMemObjects(const uintptr_t *input, uint32_t numOfObjects);

  protected:
    bool dumpKernelArgsEnabled = false;
    FileLogger<debugLevel> &baseLogger;
};

ClFileLogger<globalDebugFunctionalityLevel> &getClFileLogger();
}; // namespace NEO
