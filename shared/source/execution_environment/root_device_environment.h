/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/sip_kernel_type.h"
#include "shared/source/helpers/affinity_mask.h"
#include "shared/source/helpers/options.h"

#include <memory>
#include <mutex>

namespace NEO {
class AubCenter;
class BindlessHeapsHelper;
class BuiltIns;
class CompilerInterface;
class Debugger;
class Device;
class ExecutionEnvironment;
class GmmClientContext;
class GmmHelper;
class GmmPageTableMngr;
class HwDeviceId;
class MemoryManager;
class MemoryOperationsHandler;
class OSInterface;
class OSTime;
class SipKernel;
class SWTagsManager;
class ProductHelper;
class GfxCoreHelper;
class ApiGfxCoreHelper;

struct HardwareInfo;

struct RootDeviceEnvironment {
  protected:
    std::unique_ptr<HardwareInfo> hwInfo;

  public:
    RootDeviceEnvironment(ExecutionEnvironment &executionEnvironment);
    RootDeviceEnvironment(RootDeviceEnvironment &) = delete;
    MOCKABLE_VIRTUAL ~RootDeviceEnvironment();

    MOCKABLE_VIRTUAL const HardwareInfo *getHardwareInfo() const;
    HardwareInfo *getMutableHardwareInfo() const;
    void setHwInfoAndInitHelpers(const HardwareInfo *hwInfo);
    bool isFullRangeSvm() const;

    MOCKABLE_VIRTUAL void initAubCenter(bool localMemoryEnabled, const std::string &aubFileName, CommandStreamReceiverType csrType);
    bool initOsInterface(std::unique_ptr<HwDeviceId> &&hwDeviceId, uint32_t rootDeviceIndex);
    void initOsTime();
    void initGmm();
    void initDebugger();
    void initDebuggerL0(Device *neoDevice);

    MOCKABLE_VIRTUAL void prepareForCleanup() const;
    MOCKABLE_VIRTUAL bool initAilConfiguration();
    GmmHelper *getGmmHelper() const;
    GmmClientContext *getGmmClientContext() const;
    MOCKABLE_VIRTUAL CompilerInterface *getCompilerInterface();
    BuiltIns *getBuiltIns();
    BindlessHeapsHelper *getBindlessHeapsHelper() const;
    void createBindlessHeapsHelper(MemoryManager *memoryManager, bool availableDevices, uint32_t rootDeviceIndex, DeviceBitfield deviceBitfield);
    void limitNumberOfCcs(uint32_t numberOfCcs);
    bool isNumberOfCcsLimited() const;

    void initHelpers();
    void initApiGfxCoreHelper();
    template <typename HelperType>
    HelperType &getHelper() const;
    const ProductHelper &getProductHelper() const;

    std::unique_ptr<SipKernel> sipKernels[static_cast<uint32_t>(SipKernelType::COUNT)];
    std::unique_ptr<GmmHelper> gmmHelper;
    std::unique_ptr<OSInterface> osInterface;
    std::unique_ptr<MemoryOperationsHandler> memoryOperationsInterface;
    std::unique_ptr<AubCenter> aubCenter;
    std::unique_ptr<BindlessHeapsHelper> bindlessHeapsHelper;
    std::unique_ptr<OSTime> osTime;

    std::unique_ptr<CompilerInterface> compilerInterface;
    std::unique_ptr<BuiltIns> builtins;
    std::unique_ptr<Debugger> debugger;
    std::unique_ptr<SWTagsManager> tagsManager;
    std::unique_ptr<ApiGfxCoreHelper> apiGfxCoreHelper;
    ExecutionEnvironment &executionEnvironment;

    AffinityMaskHelper deviceAffinityMask{true};

  protected:
    bool limitedNumberOfCcs = false;

  private:
    std::mutex mtx;
};

} // namespace NEO
