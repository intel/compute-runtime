/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "sysman/engine/os_engine.h"
#include "sysman/linux/fs_access.h"
#include "sysman/linux/os_sysman_imp.h"
namespace L0 {

class LinuxEngineImp : public OsEngine, public NEO::NonCopyableClass {
  public:
    ze_result_t getActiveTime(uint64_t &activeTime) override;
    ze_result_t getEngineGroup(zet_engine_group_t &engineGroup) override;

    LinuxEngineImp() = default;
    LinuxEngineImp(OsSysman *pOsSysman);
    ~LinuxEngineImp() override = default;

  protected:
    SysfsAccess *pSysfsAccess = nullptr;
    static const std::string computeEngineGroupFile;
    static const std::string computeEngineGroupName;
};

} // namespace L0
