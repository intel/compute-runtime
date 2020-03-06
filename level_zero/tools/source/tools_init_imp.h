/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/tools_init.h"

#include <mutex>

namespace L0 {

class ToolsInitImp : public ToolsInit {
  public:
    ze_result_t initTools(ze_init_flag_t flag) override;
    bool areToolsEnabled() override;

  private:
    std::once_flag initToolsOnce;
    bool toolsAreEnabled;
};

} // namespace L0
