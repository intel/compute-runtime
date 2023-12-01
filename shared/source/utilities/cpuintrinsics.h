/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
namespace CpuIntrinsics {

void sfence();

void clFlush(void const *ptr);

void clFlushOpt(void *ptr);

void pause();

} // namespace CpuIntrinsics
} // namespace NEO
