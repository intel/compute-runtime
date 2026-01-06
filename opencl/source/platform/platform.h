/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/common_types.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/memory_manager/unified_memory_pooling.h"
#include "shared/source/utilities/reference_tracked_object.h"
#include "shared/source/utilities/staging_buffer_manager.h"

#include "opencl/source/api/cl_types.h"
#include "opencl/source/cl_device/cl_device_vector.h"
#include "opencl/source/helpers/base_object.h"

#include <mutex>
#include <vector>

namespace NEO {

class CompilerInterface;
class Device;
class ExecutionEnvironment;
class GmmHelper;
class GmmClientContext;
class SVMAllocsManager;
class StagingBufferManager;
struct PlatformInfo;
struct HardwareInfo;
class ClDevice;

template <>
struct OpenCLObjectMapper<_cl_platform_id> {
    typedef class Platform DerivedType;
};

class Platform : public BaseObject<_cl_platform_id> {
  public:
    static const cl_ulong objectMagic = 0x8873ACDEF2342133LL;

    Platform(ExecutionEnvironment &executionEnvironment);
    ~Platform() override;

    cl_int getInfo(cl_platform_info paramName,
                   size_t paramValueSize,
                   void *paramValue,
                   size_t *paramValueSizeRet);

    MOCKABLE_VIRTUAL bool initialize(std::vector<std::unique_ptr<Device>> devices);
    bool isInitialized();
    void tryNotifyGtpinInit();

    size_t getNumDevices() const;
    ClDevice **getClDevices();
    ClDevice *getClDevice(size_t deviceOrdinal);
    void devicesCleanup(bool processTermination);

    const PlatformInfo &getPlatformInfo() const;
    ExecutionEnvironment *peekExecutionEnvironment() const { return &executionEnvironment; }

    SVMAllocsManager *getSVMAllocsManager() const;
    StagingBufferManager *getStagingBufferManager() const;
    UsmMemAllocPool &getHostMemAllocPool();
    void initializeHostUsmAllocationPool();

    void incActiveContextCount();
    void decActiveContextCount();

    static std::unique_ptr<Platform> (*createFunc)(ExecutionEnvironment &executionEnvironment);

  protected:
    enum {
        StateNone,
        StateIniting,
        StateInited,
    };
    cl_uint state = StateNone;
    void fillGlobalDispatchTable();
    std::unique_ptr<PlatformInfo> platformInfo;
    ClDeviceVector clDevices;
    ExecutionEnvironment &executionEnvironment;
    std::once_flag initializeExtensionsWithVersionOnce;
    std::once_flag oclInitGTPinOnce;
    SVMAllocsManager *svmAllocsManager = nullptr;
    StagingBufferManager *stagingBufferManager = nullptr;
    int32_t activeContextCount = 0;
    UsmMemAllocPool usmHostMemAllocPool;
    bool usmPoolInitialized = false;
};

static_assert(NEO::NonCopyableAndNonMovable<BaseObject<_cl_platform_id>>);
static_assert(NEO::NonCopyableAndNonMovable<Platform>);

extern std::vector<std::unique_ptr<Platform>> *platformsImpl;
} // namespace NEO
