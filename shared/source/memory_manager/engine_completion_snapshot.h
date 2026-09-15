/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/task_count_helper.h"
#include "shared/source/utilities/stackvec.h"

#include <utility>

namespace NEO {
class CommandStreamReceiver;

// Task counts an allocation had on each engine at a chosen point in time. Unlike a
// live allocInUse() check, it does not grow with work submitted after the snapshot.
using EngineCompletionSnapshot = StackVec<std::pair<CommandStreamReceiver *, TaskCountType>, 8>;

// Polls every partition tag directly instead of going through testTaskCountReady, which
// pauses and yields when the work is not done - too costly for a check that runs on the
// free path. A tag that reads low only delays the answer to the next call.
bool isEngineCompletionSnapshotReady(const EngineCompletionSnapshot &snapshot);

} // namespace NEO
