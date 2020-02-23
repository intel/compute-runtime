/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "gen11/hw_cmds.h"
#include "helpers/windows/gmm_callbacks.h"
#include "helpers/windows/gmm_callbacks.inl"

using namespace NEO;

template struct DeviceCallbacks<ICLFamily>;
template struct TTCallbacks<ICLFamily>;
