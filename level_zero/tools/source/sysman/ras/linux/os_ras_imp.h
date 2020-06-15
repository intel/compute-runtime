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
class LinuxRasImp : public OsRas, public NEO::NonCopyableClass {
  public:
    LinuxRasImp(OsSysman *pOsSysman);
    LinuxRasImp() = default;
    ~LinuxRasImp() override = default;
    ze_result_t getCounterValues(zet_ras_details_t *pDetails) override;
    bool isRasSupported(void) override;
    void setRasErrorType(zet_ras_error_type_t rasErrorType) override;

  protected:
    FsAccess *pFsAccess = nullptr;
    zet_ras_error_type_t osRasErrorType;

  private:
    static const std::string rasCounterDir;
    static const std::string resetCounter;
    static const std::string resetCounterFile;
    std::vector<std::string> rasCounterDirFileList = {};
};

} // namespace L0
