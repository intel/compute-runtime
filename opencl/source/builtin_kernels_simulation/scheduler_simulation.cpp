/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/builtin_kernels_simulation/scheduler_simulation.h"

#include "opencl/source/builtin_kernels_simulation/opencl_c.h"

#include <thread>

using namespace NEO;

namespace BuiltinKernelsSimulation {

bool conditionReady = false;
std::thread threads[NUM_OF_THREADS];

} // namespace BuiltinKernelsSimulation
