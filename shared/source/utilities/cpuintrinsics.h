/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
namespace CpuIntrinsics {

void sfence();

void clFlush(void const *ptr);

void pause();

} // namespace CpuIntrinsics
} // namespace NEO
