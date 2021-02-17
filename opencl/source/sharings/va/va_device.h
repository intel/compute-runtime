/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "CL/cl_va_api_media_sharing_intel.h"

#include <functional>

namespace NEO {
class ClDevice;
class Platform;

class VADevice {
  public:
    VADevice();
    virtual ~VADevice();

    ClDevice *getRootDeviceFromVaDisplay(Platform *pPlatform, VADisplay vaDisplay);
    ClDevice *getDeviceFromVA(Platform *pPlatform, VADisplay vaDisplay);

    static std::function<void *(const char *, int)> fdlopen;
    static std::function<void *(void *handle, const char *symbol)> fdlsym;
    static std::function<int(void *handle)> fdlclose;

  protected:
    void *vaLibHandle = nullptr;
    void *vaGetDevice = nullptr;
};
} // namespace NEO
