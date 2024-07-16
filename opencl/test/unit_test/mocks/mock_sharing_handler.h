/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "opencl/source/sharings/sharing.h"

class MockSharingHandler : public SharingHandler {
  public:
    void synchronizeObject(UpdateData &updateData) override {
        updateData.synchronizationStatus = ACQUIRE_SUCCESFUL;
    }
};
