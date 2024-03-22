/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/kmd_notify_properties.h"

namespace NEO {
class MockKmdNotifyHelper : public KmdNotifyHelper {
  public:
    using KmdNotifyHelper::acLineConnected;
    using KmdNotifyHelper::getMicrosecondsSinceEpoch;
    using KmdNotifyHelper::lastWaitForCompletionTimestampUs;
    using KmdNotifyHelper::properties;

    MockKmdNotifyHelper() = delete;
    MockKmdNotifyHelper(const KmdNotifyProperties *newProperties) : KmdNotifyHelper(newProperties){};

    void updateLastWaitForCompletionTimestamp() override {
        KmdNotifyHelper::updateLastWaitForCompletionTimestamp();
        updateLastWaitForCompletionTimestampCalled++;
    }

    void updateAcLineStatus() override {
        KmdNotifyHelper::updateAcLineStatus();
        updateAcLineStatusCalled++;
    }

    uint32_t updateLastWaitForCompletionTimestampCalled = 0u;
    uint32_t updateAcLineStatusCalled = 0u;
};
} // namespace NEO