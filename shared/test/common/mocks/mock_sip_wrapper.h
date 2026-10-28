/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"

#include <cstddef>

namespace NEO {

class MockSipWrapper {
  public:
    MockSipWrapper();

  protected:
    DebugManagerStateRestore dbgRestore;
    VariableBackup<size_t> mockFreadReturnBackup;
    VariableBackup<long> mockFtellReturnBackup;
    VariableBackup<char *> mockFreadBufferBackup;
};

} // namespace NEO
