/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/os_interface/os_interface.h"

#include <string>
namespace NEO {

class HwDeviceIdDrm : public HwDeviceId {
  public:
    static constexpr DriverModelType driverModelType = DriverModelType::DRM;

    HwDeviceIdDrm(int fileDescriptorIn, const char *pciPathIn)
        : HwDeviceId(DriverModelType::DRM),
          fileDescriptor(fileDescriptorIn), pciPath(pciPathIn) {}
    HwDeviceIdDrm(int fileDescriptorIn, const char *pciPathIn, const char *devNodePathIn)
        : HwDeviceId(DriverModelType::DRM),
          fileDescriptor(fileDescriptorIn), pciPath(pciPathIn), devNodePath(devNodePathIn) {}
    ~HwDeviceIdDrm() override;
    int getFileDescriptor() const { return fileDescriptor; }
    const char *getPciPath() const { return pciPath.c_str(); }
    const char *getDeviceNode() const { return devNodePath.c_str(); }

  protected:
    int fileDescriptor;
    const std::string pciPath;
    const std::string devNodePath;
};
} // namespace NEO
