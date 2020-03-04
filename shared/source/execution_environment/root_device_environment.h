/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/options.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace NEO {

class BuiltIns;
class CompilerInterface;
class AubCenter;
class GmmClientContext;
class GmmHelper;
class ExecutionEnvironment;
class GmmPageTableMngr;
class MemoryOperationsHandler;
class OSInterface;
struct HardwareInfo;
class HwDeviceId;

struct RootDeviceEnvironment {
  protected:
    std::unique_ptr<HardwareInfo> hwInfo;

  public:
    RootDeviceEnvironment(ExecutionEnvironment &executionEnvironment);
    RootDeviceEnvironment(RootDeviceEnvironment &) = delete;
    MOCKABLE_VIRTUAL ~RootDeviceEnvironment();

    MOCKABLE_VIRTUAL const HardwareInfo *getHardwareInfo() const;
    HardwareInfo *getMutableHardwareInfo() const;
    void setHwInfo(const HardwareInfo *hwInfo);
    bool isFullRangeSvm() const;

    MOCKABLE_VIRTUAL void initAubCenter(bool localMemoryEnabled, const std::string &aubFileName, CommandStreamReceiverType csrType);
    bool initOsInterface(std::unique_ptr<HwDeviceId> &&hwDeviceId);
    void initGmm();
    GmmHelper *getGmmHelper() const;
    GmmClientContext *getGmmClientContext() const;
    MOCKABLE_VIRTUAL CompilerInterface *getCompilerInterface();
    BuiltIns *getBuiltIns();

    std::unique_ptr<GmmHelper> gmmHelper;
    std::unique_ptr<OSInterface> osInterface;
    std::unique_ptr<GmmPageTableMngr> pageTableManager;
    std::unique_ptr<MemoryOperationsHandler> memoryOperationsInterface;
    std::unique_ptr<AubCenter> aubCenter;

    std::unique_ptr<BuiltIns> builtins;
    std::unique_ptr<CompilerInterface> compilerInterface;
    ExecutionEnvironment &executionEnvironment;

  private:
    std::mutex mtx;
};
} // namespace NEO
