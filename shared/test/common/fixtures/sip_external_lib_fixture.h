/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

class MockSipExternalLib;

namespace NEO {
class Device;

struct SipExternalLibFixture {
    void injectSipLib(Device &device);

    MockSipExternalLib *sipExternalLib = nullptr;
};

void injectSipLibIfRequired(Device &device);
} // namespace NEO
