/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

struct AubSubCaptureStatus {
    bool isActive;
    bool wasActiveInPreviousEnqueue;
};
} // namespace NEO
