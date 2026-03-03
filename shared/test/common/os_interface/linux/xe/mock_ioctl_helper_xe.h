/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"
#include "shared/source/os_interface/linux/xe/xe_log_helper.h"

using namespace NEO;

struct MockIoctlHelperXe : IoctlHelperXe {
    using IoctlHelperXe::bindInfo;
    using IoctlHelperXe::contextParamEngine;
    using IoctlHelperXe::defaultEngine;
    using IoctlHelperXe::euDebugInterface;
    using IoctlHelperXe::getDefaultEngineClass;
    using IoctlHelperXe::getFdFromVmExport;
    using IoctlHelperXe::getPrimaryContextId;
    using IoctlHelperXe::ioctl;
    using IoctlHelperXe::IoctlHelperXe;
    using IoctlHelperXe::isLowLatencyHintAvailable;
    using IoctlHelperXe::maxContextSetProperties;
    using IoctlHelperXe::maxExecQueuePriority;
    using IoctlHelperXe::queryGtListData;
    using IoctlHelperXe::queryHwIpVersion;
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

    bool isVmBindDecompressAvailable(uint32_t vmId) override { return false; }

    void testLog(auto &&...args) {
        XELOG(args...);
    }

    int createDrmContext(Drm &drm, OsContextLinux &osContext, uint32_t drmVmId, uint32_t deviceIndex, bool allocateInterrupt) override {
        if (mockCreateDrmContext) {
            return 1;
        }
        return IoctlHelperXe::createDrmContext(drm, osContext, drmVmId, deviceIndex, allocateInterrupt);
    }

    int bindAddDebugData(std::vector<VmBindOpExtDebugData> debugDataVec, uint32_t vmId, VmBindExtUserFenceT *vmBindExtUserFence, bool isAdd) override {
        bindAddDebugDataCalled = true;
        bindVmId = vmId;
        bindAdd = isAdd;

        bindAddDebugDataVec = debugDataVec;
        std::memcpy(&bindAddExtUserFence, vmBindExtUserFence, sizeof(VmBindExtUserFenceT));

        if (failBindAddDebugData) {
            return -1;
        }

        if (mockBindAddDebugData) {
            return 0;
        }

        return IoctlHelperXe::bindAddDebugData(debugDataVec, vmId, vmBindExtUserFence, isAdd);
    }

    std::optional<std::vector<VmBindOpExtDebugData>> addDebugDataAndCreateBindOpVec(BufferObject *bo, uint32_t vmId, bool isAdd) override {
        addDebugDataAndCreateBindOpVecCalled = true;
        addDebugDataVmId = vmId;
        addDebugAdd = isAdd;

        if (failAddDebugDataAndCreateBindOpVec) {
            return std::nullopt;
        }

        if (mockAddDebugDataAndCreateBindOpVec) {
            std::vector<VmBindOpExtDebugData> debugDataVec;
            VmBindOpExtDebugData debugData{};
            debugData.base.name = 0;
            debugData.base.nextExtension = 0;
            debugData.base.pad = 0;
            debugData.addr = 0x1234;
            debugData.range = 0x1000;
            debugData.flags = 1;
            debugData.pseudopath = 0x1;
            debugDataVec.push_back(debugData);
            return debugDataVec;
        }
        return IoctlHelperXe::addDebugDataAndCreateBindOpVec(bo, vmId, isAdd);
    }

    bool failPerfDisable = false;
    bool failPerfEnable = false;
    bool failPerfOpen = false;
    bool failPerfQuery = false;

    bool failBindAddDebugData = false;
    bool bindAddDebugDataCalled = false;
    bool failAddDebugDataAndCreateBindOpVec = false;
    bool addDebugDataAndCreateBindOpVecCalled = false;
    bool mockAddDebugDataAndCreateBindOpVec = false;
    bool mockCreateDrmContext = false;
    bool mockBindAddDebugData = false;
    uint64_t fenceAddr = 0;
    uint64_t fenceVal = 0;

    uint32_t bindVmId = 0;
    uint32_t addDebugDataVmId = 0;
    bool bindAdd = false;
    bool addDebugAdd = false;

    std::vector<VmBindOpExtDebugData> bindAddDebugDataVec;
    VmBindExtUserFenceT bindAddExtUserFence{};
};
