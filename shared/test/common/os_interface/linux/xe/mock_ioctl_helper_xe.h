/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"

using namespace NEO;

struct MockIoctlHelperXe : IoctlHelperXe {
    using IoctlHelperXe::bindInfo;
    using IoctlHelperXe::contextParamEngine;
    using IoctlHelperXe::defaultEngine;
    using IoctlHelperXe::getDefaultEngineClass;
    using IoctlHelperXe::getFdFromVmExport;
    using IoctlHelperXe::ioctl;
    using IoctlHelperXe::IoctlHelperXe;
    using IoctlHelperXe::isLowLatencyHintAvailable;
    using IoctlHelperXe::maxContextSetProperties;
    using IoctlHelperXe::maxExecQueuePriority;
    using IoctlHelperXe::queryGtListData;
    using IoctlHelperXe::setContextProperties;
    using IoctlHelperXe::tileIdToGtId;
    using IoctlHelperXe::updateBindInfo;
    using IoctlHelperXe::UserFenceExtension;
    using IoctlHelperXe::xeGetAdviseOperationName;
    using IoctlHelperXe::xeGetBindFlagNames;
    using IoctlHelperXe::xeGetBindOperationName;
    using IoctlHelperXe::xeGetClassName;
    using IoctlHelperXe::xeGetengineClassName;
    using IoctlHelperXe::xeGtListData;
    using IoctlHelperXe::xeShowBindTable;

    int perfOpenIoctl(DrmIoctl request, void *arg) override {
        switch (request) {
        case DrmIoctl::perfQuery:
            if (failPerfQuery) {
                return -1;
            }
            break;
        case DrmIoctl::perfOpen:
            if (failPerfOpen) {
                return -1;
            }
            break;
        default:
            break;
        }
        return IoctlHelperXe::perfOpenIoctl(request, arg);
    }

    int ioctl(int fd, DrmIoctl request, void *arg) override {
        if (request == DrmIoctl::perfDisable) {
            if (failPerfDisable) {
                return -1;
            }
            return 0;
        }
        if (request == DrmIoctl::perfEnable) {
            if (failPerfEnable) {
                return -1;
            }
            return 0;
        }
        return IoctlHelperXe::ioctl(fd, request, arg);
    }

    bool failPerfDisable = false;
    bool failPerfEnable = false;
    bool failPerfOpen = false;
    bool failPerfQuery = false;
};
