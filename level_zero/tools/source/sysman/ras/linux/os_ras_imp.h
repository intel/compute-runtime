/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/ras/os_ras.h"

#include <string>
#include <vector>

namespace L0 {

class FsAccess;
class LinuxRasImp : public OsRas, NEO::NonCopyableOrMovableClass {
  public:
    LinuxRasImp(OsSysman *pOsSysman);
    LinuxRasImp() = default;
    ~LinuxRasImp() override = default;
    bool isRasSupported(void) override;
    void setRasErrorType(zes_ras_error_type_t rasErrorType) override;

  protected:
    FsAccess *pFsAccess = nullptr;
    zes_ras_error_type_t osRasErrorType;

  private:
    static const std::string rasCounterDir;
    static const std::string resetCounter;
    static const std::string resetCounterFile;
    std::vector<std::string> rasCounterDirFileList = {};
};

} // namespace L0
