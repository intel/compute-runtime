/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debug_settings/debug_settings_manager.h"

#include <cstddef>

using namespace NEO;

// Records the previous value of every set() on the DebugVariables passed to startRecording().
// DebugManagerStateRestore reverts the changes made during its lifetime, stopRecording() reverts the rest.
namespace DebugVariablesChangeTracker {
void startRecording(DebugVariables &flags);
void stopRecording(void (*reportChangedVariable)(const void *variable));
} // namespace DebugVariablesChangeTracker

class DebugManagerStateRestore {
  public:
    DebugManagerStateRestore();
    ~DebugManagerStateRestore();
    DebugManagerStateRestore(const DebugManagerStateRestore &) = delete;
    DebugManagerStateRestore &operator=(const DebugManagerStateRestore &) = delete;

  protected:
    size_t firstChangeIndex;
    void *injectFcnSnapshot;
};
