/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/options.h"

#include <cstdint>
#include <memory>
#include <string>

namespace NEO {

class AubCenter;
class ExecutionEnvironment;
class GmmPageTableMngr;
class MemoryOperationsHandler;
class OSInterface;

struct RootDeviceEnvironment {
    RootDeviceEnvironment(ExecutionEnvironment &executionEnvironment);
    RootDeviceEnvironment(RootDeviceEnvironment &) = delete;
    MOCKABLE_VIRTUAL ~RootDeviceEnvironment();

    MOCKABLE_VIRTUAL void initAubCenter(bool localMemoryEnabled, const std::string &aubFileName, CommandStreamReceiverType csrType);

    std::unique_ptr<OSInterface> osInterface;
    std::unique_ptr<GmmPageTableMngr> pageTableManager;
    std::unique_ptr<MemoryOperationsHandler> memoryOperationsInterface;
    std::unique_ptr<AubCenter> aubCenter;
    ExecutionEnvironment &executionEnvironment;
};
} // namespace NEO
