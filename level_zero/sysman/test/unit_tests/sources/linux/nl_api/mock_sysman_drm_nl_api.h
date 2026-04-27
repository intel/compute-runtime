/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/shared/linux/nl_api/sysman_drm_nl_api.h"

namespace L0 {
namespace Sysman {
namespace ult {

class MockDrmNlApi : public L0::Sysman::DrmNlApi {
  public:
    using DrmNlApi::currentOperation;
    using DrmNlApi::handleAck;
    using DrmNlApi::initConnection;
    using DrmNlApi::Operation;
    using DrmNlApi::pNlApi;
    MockDrmNlApi(std::string devId) : L0::Sysman::DrmNlApi() {}
    ~MockDrmNlApi() override = default;
    ze_result_t listNodesReturnStatus = ZE_RESULT_SUCCESS;
    ze_result_t getErrorCounterReturnStatus = ZE_RESULT_SUCCESS;
    ze_result_t getErrorsListReturnStatus = ZE_RESULT_SUCCESS;
    ze_result_t clearErrorCounterReturnStatus = ZE_RESULT_SUCCESS;
    bool getMockErrorList = false;
    bool callRealListNodes = false;
    bool returnEmptyErrorList = false;
    bool returnAllErrorList = false;
    std::vector<DrmRasNode> mockListNodesReturnData;

    ze_result_t getErrorsList(const uint32_t &nodeId, std::vector<DrmErrorCounter> &errorList) override {
        if (getErrorsListReturnStatus != ZE_RESULT_SUCCESS) {
            return getErrorsListReturnStatus;
        }

        if (returnEmptyErrorList) {
            return ZE_RESULT_SUCCESS;
        }

        if (returnAllErrorList) {
            std::vector<std::pair<std::string, uint32_t>> allErrors = {
                {"core-compute", 10},
                {"device-memory", 20},
                {"fabric", 30},
                {"scale", 40},
                {"pcie", 50},
                {"soc-internal", 60}};
            for (const auto &e : allErrors) {
                DrmErrorCounter counter = {};
                counter.nodeId = nodeId;
                counter.errorName = e.first;
                counter.errorValue = e.second;
                errorList.push_back(counter);
            }
            return ZE_RESULT_SUCCESS;
        }

        if (getMockErrorList) {
            DrmErrorCounter error1 = {};
            error1.nodeId = nodeId;
            error1.errorId = 1;
            error1.errorName = "core-compute";
            error1.errorValue = 10;

            DrmErrorCounter error2 = {};
            error2.nodeId = nodeId;
            error2.errorId = 2;
            error2.errorName = "soc-internal";
            error2.errorValue = 20;

            errorList.push_back(error1);
            errorList.push_back(error2);

            return ZE_RESULT_SUCCESS;
        }

        return DrmNlApi::getErrorsList(nodeId, errorList);
    }

    ze_result_t listNodes(std::vector<DrmRasNode> &nodeList) override {
        if (callRealListNodes) {
            return DrmNlApi::listNodes(nodeList);
        }
        if (listNodesReturnStatus != ZE_RESULT_SUCCESS) {
            return listNodesReturnStatus;
        }

        if (!mockListNodesReturnData.empty()) {
            nodeList = mockListNodesReturnData;
            return ZE_RESULT_SUCCESS;
        }

        DrmRasNode node1 = {};
        node1.nodeId = 0;
        node1.deviceName = "0000:4d:00.0";
        node1.nodeName = "correctable-errors";
        node1.nodeType = 1;

        DrmRasNode node2 = {};
        node2.nodeId = 1;
        node2.deviceName = "0000:4d:00.0";
        node2.nodeName = "uncorrectable-errors";
        node2.nodeType = 1;

        nodeList.push_back(node1);
        nodeList.push_back(node2);

        return ZE_RESULT_SUCCESS;
    }

    ze_result_t clearErrorCounter(const uint32_t &nodeId, const uint32_t &errorId) override {
        if (clearErrorCounterReturnStatus != ZE_RESULT_SUCCESS) {
            return clearErrorCounterReturnStatus;
        }

        return DrmNlApi::clearErrorCounter(nodeId, errorId);
    }
};

} // namespace ult
} // namespace Sysman
} // namespace L0
