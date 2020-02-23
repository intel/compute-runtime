/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "gen8/hw_cmds.h"
#include "helpers/windows/gmm_callbacks.h"
#include "helpers/windows/gmm_callbacks.inl"

using namespace NEO;

template struct DeviceCallbacks<BDWFamily>;
template struct TTCallbacks<BDWFamily>;
