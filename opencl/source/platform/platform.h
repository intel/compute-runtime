/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/api/cl_types.h"
#include "opencl/source/cl_device/cl_device_vector.h"
#include "opencl/source/helpers/base_object.h"

#include "platform_info.h"

#include <condition_variable>
#include <unordered_map>
#include <vector>

namespace NEO {

class CompilerInterface;
class Device;
class ExecutionEnvironment;
class GmmHelper;
class GmmClientContext;
struct HardwareInfo;

template <>
struct OpenCLObjectMapper<_cl_platform_id> {
    typedef class Platform DerivedType;
};

using DeviceVector = std::vector<std::unique_ptr<Device>>;
class Platform : public BaseObject<_cl_platform_id> {
  public:
    static const cl_ulong objectMagic = 0x8873ACDEF2342133LL;

    Platform(ExecutionEnvironment &executionEnvironment);
    ~Platform() override;

    Platform(const Platform &) = delete;
    Platform &operator=(Platform const &) = delete;

    cl_int getInfo(cl_platform_info paramName,
                   size_t paramValueSize,
                   void *paramValue,
                   size_t *paramValueSizeRet);

    MOCKABLE_VIRTUAL bool initialize(std::vector<std::unique_ptr<Device>> devices);
    bool isInitialized();

    size_t getNumDevices() const;
    ClDevice **getClDevices();
    ClDevice *getClDevice(size_t deviceOrdinal);

    const PlatformInfo &getPlatformInfo() const;
    ExecutionEnvironment *peekExecutionEnvironment() const { return &executionEnvironment; }

    static std::unique_ptr<Platform> (*createFunc)(ExecutionEnvironment &executionEnvironment);
    static std::vector<DeviceVector> groupDevices(DeviceVector devices);

  protected:
    enum {
        StateNone,
        StateIniting,
        StateInited,
    };
    cl_uint state = StateNone;
    void fillGlobalDispatchTable();
    MOCKABLE_VIRTUAL void initializationLoopHelper(){};
    std::unique_ptr<PlatformInfo> platformInfo;
    ClDeviceVector clDevices;
    ExecutionEnvironment &executionEnvironment;
};

extern std::vector<std::unique_ptr<Platform>> platformsImpl;
} // namespace NEO
