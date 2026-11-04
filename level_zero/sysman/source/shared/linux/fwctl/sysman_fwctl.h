/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/zes_api.h"

#include <fwctl/fwctl.h>
#include <fwctl/xe_sysctrl.h>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace L0 {
namespace Sysman {

class LinuxSysmanImp;

class FwctlInterface : NEO::NonCopyableAndNonMovableClass {
  public:
    static std::unique_ptr<FwctlInterface> create(LinuxSysmanImp *pLinuxSysmanImp);

    FwctlInterface(LinuxSysmanImp *pLinuxSysmanImp) : pLinuxSysmanImp(pLinuxSysmanImp) {}
    virtual ~FwctlInterface() = default;

    virtual ze_result_t sendSysctrlRpc(fwctl_xe_sysctrl_uctx_caps requiredCap, fwctl_rpc_scope scope, fwctl_rpc_xe_sysctrl &rpc);

  protected:
    virtual ze_result_t openSysctrlNode(int &fd, uint32_t &uctxCaps);
    virtual ze_result_t discoverSysctrlNode(int &fd, std::string &nodePath, uint32_t &uctxCaps);
    virtual ze_result_t getNodeNames(std::vector<std::string> &nodeNames);
    void invalidateSysctrlNode();
    static ze_result_t getFwctlResult(int err);

    LinuxSysmanImp *pLinuxSysmanImp = nullptr;

    std::mutex nodeLock;
    std::string sysctrlNodePath;
    uint32_t sysctrlUctxCaps = 0;
};

} // namespace Sysman
} // namespace L0
