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

class AubCenter;
class BuiltIns;
class CompilerInterface;
class Debugger;
class ExecutionEnvironment;
class GmmClientContext;
class GmmHelper;
class GmmPageTableMngr;
class HwDeviceId;
class MemoryOperationsHandler;
class OSInterface;
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
    void setHwInfo(const HardwareInfo *hwInfo);
    bool isFullRangeSvm() const;

    MOCKABLE_VIRTUAL void initAubCenter(bool localMemoryEnabled, const std::string &aubFileName, CommandStreamReceiverType csrType);
    bool initOsInterface(std::unique_ptr<HwDeviceId> &&hwDeviceId);
    void initGmm();
    void initDebugger();
    GmmHelper *getGmmHelper() const;
    GmmClientContext *getGmmClientContext() const;
    MOCKABLE_VIRTUAL CompilerInterface *getCompilerInterface();
    BuiltIns *getBuiltIns();

    std::unique_ptr<GmmHelper> gmmHelper;
    std::unique_ptr<OSInterface> osInterface;
    std::unique_ptr<GmmPageTableMngr> pageTableManager;
    std::unique_ptr<MemoryOperationsHandler> memoryOperationsInterface;
    std::unique_ptr<AubCenter> aubCenter;

    std::unique_ptr<CompilerInterface> compilerInterface;
    std::unique_ptr<BuiltIns> builtins;
    std::unique_ptr<Debugger> debugger;
    ExecutionEnvironment &executionEnvironment;

  private:
    std::mutex mtx;
};
} // namespace NEO
