/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

class CrtAssertReportSuppression {
  public:
    CrtAssertReportSuppression();
    ~CrtAssertReportSuppression();

  private:
    [[maybe_unused]] int previousReportMode = 0;
};
