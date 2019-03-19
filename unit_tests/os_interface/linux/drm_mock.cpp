/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/os_interface/linux/drm_mock.h"

int DrmMock::handleRemainingRequests(unsigned long request, void *arg) {
    return -1;
};
