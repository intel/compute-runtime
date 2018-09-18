/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/aub_tests/aub_tests_configuration.inl"
#include "runtime/gen_common/hw_cmds.h"

using namespace OCLRT;

template AubTestsConfig GetAubTestsConfig<BDWFamily>();
