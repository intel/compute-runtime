/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/device_bitfield.h"
#include "shared/source/utilities/stackvec.h"

#include "level_zero/api/opencl/source/api/cl_types.h"
#include "level_zero/api/opencl/source/cl_device/cl_device.h"
#include "level_zero/api/opencl/source/helpers/base_object.h"
#include "level_zero/api/opencl/source/platform/platform_info.h"
#include "level_zero/core/source/driver/driver_handle.h"

#include <map>
#include <vector>

namespace NEO {
namespace LEO {

template <>
struct OpenCLObjectMapper<_cl_platform_id> {
    typedef class Platform DerivedType;
};

class Platform : public BaseObject<_cl_platform_id> {
  public:
    static constexpr cl_ulong objectMagic = 0x8873ACDEF2342133LL;

    Platform(ze_driver_handle_t driverHandle);
    Platform() = delete;

    cl_int getInfo(cl_platform_info paramName,
                   size_t paramValueSize,
                   void *paramValue,
                   size_t *paramValueSizeRet);

    const std::vector<std::unique_ptr<ClDevice>> &getDevices() const { return this->clDevices; };
    ze_driver_handle_t getL0Handle() const { return this->driverHandle; }
    L0::DriverHandle *getL0Object() const { return L0::DriverHandle::fromHandle(this->driverHandle); }

    const RootDeviceIndicesContainer &getRootDeviceIndices() const { return this->rootDeviceIndices; };
    const std::map<uint32_t, DeviceBitfield> &getDeviceBitfields() const { return this->deviceBitfields; };

  protected:
    std::unique_ptr<PlatformInfo> platformInfo{};
    std::once_flag initializeExtensionsWithVersionOnce;

    std::vector<std::unique_ptr<ClDevice>> clDevices{};

    RootDeviceIndicesContainer rootDeviceIndices{};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{};

    ze_driver_handle_t driverHandle = nullptr;
};

static_assert(NEO::NonCopyableAndNonMovable<BaseObject<_cl_platform_id>>);
static_assert(NEO::NonCopyableAndNonMovable<Platform>);

extern std::vector<std::unique_ptr<Platform>> *platformsImpl;

} // namespace LEO
} // namespace NEO
