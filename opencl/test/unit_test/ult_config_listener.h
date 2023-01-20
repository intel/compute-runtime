/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/base_ult_config_listener.h"

namespace NEO {

class UltConfigListener : public BaseUltConfigListener {
  private:
    void OnTestStart(const ::testing::TestInfo &) override;
    void OnTestEnd(const ::testing::TestInfo &) override;
};

} // namespace NEO
