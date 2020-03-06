/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/ze_api.h>

namespace L0 {

class ToolsInit {
  public:
    virtual ze_result_t initTools(ze_init_flag_t flag) = 0;
    virtual bool areToolsEnabled() = 0;
    static ToolsInit *get() { return toolsInit; }

  protected:
    static ToolsInit *toolsInit;
};

} // namespace L0
