/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/non_copyable_or_moveable.h"

namespace NEO {

class HwDeviceId : NonCopyableClass {
  public:
    HwDeviceId(int fileDescriptorIn) : fileDescriptor(fileDescriptorIn) {}
    ~HwDeviceId();
    int getFileDescriptor() const { return fileDescriptor; }

  protected:
    const int fileDescriptor;
};
} // namespace NEO
