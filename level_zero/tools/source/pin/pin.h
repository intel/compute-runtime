/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

namespace L0 {

class PinContext {
  public:
    static void init(ze_init_flag_t flag);
};

} // namespace L0
