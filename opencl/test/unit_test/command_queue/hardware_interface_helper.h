/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/command_queue/hardware_interface.h"

inline NEO::HardwareInterfaceWalkerArgs createHardwareInterfaceWalkerArgs(size_t wkgSizeArray[3], Vec3<size_t> &wgInfo) {
    NEO::HardwareInterfaceWalkerArgs args = {};
    args.globalWorkSizes[0] = args.localWorkSizes[0] = wkgSizeArray[0];
    args.globalWorkSizes[1] = args.localWorkSizes[1] = wkgSizeArray[1];
    args.globalWorkSizes[2] = args.localWorkSizes[2] = wkgSizeArray[2];

    args.numberOfWorkgroups = &wgInfo;
    args.startOfWorkgroups = &wgInfo;

    return args;
}

inline NEO::HardwareInterfaceWalkerArgs createHardwareInterfaceWalkerArgs(size_t wkgSizeArray[3], Vec3<size_t> &wgInfo, PreemptionMode mode) {
    NEO::HardwareInterfaceWalkerArgs args = createHardwareInterfaceWalkerArgs(wkgSizeArray, wgInfo);
    args.preemptionMode = mode;

    return args;
}

inline NEO::HardwareInterfaceWalkerArgs createHardwareInterfaceWalkerArgs(uint32_t commandType) {
    NEO::HardwareInterfaceWalkerArgs args = {};
    args.commandType = commandType;
    return args;
}
