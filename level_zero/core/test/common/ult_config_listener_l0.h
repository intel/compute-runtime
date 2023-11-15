/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/base_ult_config_listener.h"

namespace L0 {

class UltConfigListenerL0 : public NEO::BaseUltConfigListener {
  private:
    void OnTestStart(const ::testing::TestInfo &) override;
    void OnTestEnd(const ::testing::TestInfo &) override;
};

} // namespace L0
