/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/program/sync_buffer_handler.h"

class MockSyncBufferHandler : public NEO::SyncBufferHandler {
  public:
    using SyncBufferHandler::bufferSize;
    using SyncBufferHandler::graphicsAllocation;
    using SyncBufferHandler::usedBufferSize;
};
