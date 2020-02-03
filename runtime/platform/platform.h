/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/api/cl_types.h"
#include "runtime/device/cl_device_vector.h"
#include "runtime/helpers/base_object.h"

#include "platform_info.h"

#include <condition_variable>
#include <unordered_map>
#include <vector>

namespace NEO {

class CompilerInterface;
class RootDevice;
class Device;
class AsyncEventsHandler;
class ExecutionEnvironment;
class GmmHelper;
class GmmClientContext;
struct HardwareInfo;

template <>
struct OpenCLObjectMapper<_cl_platform_id> {
    typedef class Platform DerivedType;
};

class Platform : public BaseObject<_cl_platform_id> {
  public:
    static const cl_ulong objectMagic = 0x8873ACDEF2342133LL;

    Platform();
    ~Platform() override;

    Platform(const Platform &) = delete;
    Platform &operator=(Platform const &) = delete;

    cl_int getInfo(cl_platform_info paramName,
                   size_t paramValueSize,
                   void *paramValue,
                   size_t *paramValueSizeRet);

    bool initialize();
    bool isInitialized();

    size_t getNumDevices() const;
    Device *getDevice(size_t deviceOrdinal);
    ClDevice **getClDevices();
    ClDevice *getClDevice(size_t deviceOrdinal);

    const PlatformInfo &getPlatformInfo() const;
    AsyncEventsHandler *getAsyncEventsHandler();
    std::unique_ptr<AsyncEventsHandler> setAsyncEventsHandler(std::unique_ptr<AsyncEventsHandler> handler);
    ExecutionEnvironment *peekExecutionEnvironment() const { return executionEnvironment; }
    GmmHelper *peekGmmHelper() const;
    GmmClientContext *peekGmmClientContext() const;

  protected:
    enum {
        StateNone,
        StateIniting,
        StateInited,
    };
    cl_uint state = StateNone;
    void fillGlobalDispatchTable();
    MOCKABLE_VIRTUAL void initializationLoopHelper(){};
    MOCKABLE_VIRTUAL RootDevice *createRootDevice(uint32_t rootDeviceIndex) const;
    std::unique_ptr<PlatformInfo> platformInfo;
    ClDeviceVector clDevices;
    std::unique_ptr<AsyncEventsHandler> asyncEventsHandler;
    ExecutionEnvironment *executionEnvironment = nullptr;
};

extern std::vector<std::unique_ptr<Platform>> platformsImpl;
Platform *platform();
Platform *constructPlatform();
} // namespace NEO
