/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "CL/cl.h"
#include "runtime/command_stream/task_count_helper.h"

#include <cstdint>

namespace NEO {
bool isGTPinInitialized = false;

void gtpinNotifyContextCreate(cl_context context) {
}

void gtpinNotifyContextDestroy(cl_context context) {
}

void gtpinNotifyKernelCreate(cl_kernel kernel) {
}

void gtpinNotifyKernelSubmit(cl_kernel kernel, void *pCmdQueue) {
}

void gtpinNotifyPreFlushTask(void *pCmdQueue) {
}

void gtpinNotifyFlushTask(TaskCountType flushedTaskCount) {
}

void gtpinNotifyTaskCompletion(TaskCountType completedTaskCount) {
}

void gtpinNotifyMakeResident(void *pKernel, void *pCommandStreamReceiver) {
}

void gtpinNotifyUpdateResidencyList(void *pKernel, void *pResidencyVector) {
}

void gtpinNotifyPlatformShutdown() {
}

void *gtpinGetIgcInit() {
    return nullptr;
}

void setIgcInfo(const void *igcInfo) {
}

const void *gtpinGetIgcInfo() {
    return nullptr;
}
} // namespace NEO
