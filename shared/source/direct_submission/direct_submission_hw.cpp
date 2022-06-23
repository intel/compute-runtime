/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/direct_submission_hw.h"

#include "shared/source/command_stream/command_stream_receiver.h"

namespace NEO {
DirectSubmissionInputParams::DirectSubmissionInputParams(const CommandStreamReceiver &commandStreamReceiver) : osContext(commandStreamReceiver.getOsContext()), rootDeviceEnvironment(commandStreamReceiver.peekRootDeviceEnvironment()), rootDeviceIndex(commandStreamReceiver.getRootDeviceIndex()) {
    memoryManager = commandStreamReceiver.getMemoryManager();
    logicalStateHelper = commandStreamReceiver.getLogicalStateHelper();
    globalFenceAllocation = commandStreamReceiver.getGlobalFenceAllocation();
    workPartitionAllocation = commandStreamReceiver.getWorkPartitionAllocation();
    completionFenceAllocation = commandStreamReceiver.getTagAllocation();
}

} // namespace NEO
