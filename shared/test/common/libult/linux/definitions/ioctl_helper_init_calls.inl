/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

uint32_t DrmMock::ioctlCallsForHelperInitialization = 0;

int DrmMock::handleRemainingRequests(DrmIoctl request, void *arg) {
    return -1;
}
