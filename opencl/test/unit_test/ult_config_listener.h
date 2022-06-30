/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/unit_test/base_ult_config_listener.h"

namespace NEO {

class UltConfigListener : public BaseUltConfigListener {
  private:
    void OnTestStart(const ::testing::TestInfo &) override;
    void OnTestEnd(const ::testing::TestInfo &) override;
};

} // namespace NEO
