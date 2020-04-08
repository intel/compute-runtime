/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <string>
namespace NEO {

class HwDeviceId : NonCopyableClass {
  public:
    HwDeviceId(int fileDescriptorIn, const char *pciPathIn) : fileDescriptor(fileDescriptorIn), pciPath(pciPathIn) {}
    ~HwDeviceId();
    int getFileDescriptor() const { return fileDescriptor; }
    const char *getPciPath() const { return pciPath.c_str(); }

  protected:
    const int fileDescriptor;
    const std::string pciPath;
};
} // namespace NEO
