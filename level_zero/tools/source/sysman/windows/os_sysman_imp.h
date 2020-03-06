/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman/sysman_imp.h"

namespace L0 {

class WddmSysmanImp : public OsSysman {
  public:
    WddmSysmanImp(SysmanImp *pParentSysmanImp) : pParentSysmanImp(pParentSysmanImp){};
    ~WddmSysmanImp() override = default;

    // Don't allow copies of the WddmSysmanImp object
    WddmSysmanImp(const WddmSysmanImp &obj) = delete;
    WddmSysmanImp &operator=(const WddmSysmanImp &obj) = delete;

    ze_result_t init() override;
    ze_result_t systemCmd(const std::string cmd, std::string &output);

  private:
    SysmanImp *pParentSysmanImp;
};

} // namespace L0
