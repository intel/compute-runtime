/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/builtin_kernels_simulation/scheduler_simulation.h"

#include "runtime/builtin_kernels_simulation/opencl_c.h"

#include <thread>

using namespace std;
using namespace NEO;

namespace BuiltinKernelsSimulation {

bool conditionReady = false;
std::thread threads[NUM_OF_THREADS];

} // namespace BuiltinKernelsSimulation
