/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
enum class TestMode { NotSpecified,
                      UnitTests,
                      AubTests,
                      AubTestsWithTbx,
                      TbxTests };

extern TestMode testMode;
} // namespace NEO
