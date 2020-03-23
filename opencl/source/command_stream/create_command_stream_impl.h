/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver.h"

namespace NEO {
class ExecutionEnvironment;
extern CommandStreamReceiver *createCommandStreamImpl(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex);
extern bool prepareDeviceEnvironmentsImpl(ExecutionEnvironment &executionEnvironment);
} // namespace NEO
