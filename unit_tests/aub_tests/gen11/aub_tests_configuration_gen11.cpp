/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gen_common/hw_cmds.h"
#include "unit_tests/aub_tests/aub_tests_configuration.inl"

using namespace NEO;

template AubTestsConfig GetAubTestsConfig<ICLFamily>();
