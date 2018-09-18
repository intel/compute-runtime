/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hw_cmds.h"
#include "runtime/helpers/gmm_callbacks.h"
#include "runtime/helpers/gmm_callbacks.inl"

using namespace OCLRT;

template struct DeviceCallbacks<CNLFamily>;
template struct TTCallbacks<CNLFamily>;
