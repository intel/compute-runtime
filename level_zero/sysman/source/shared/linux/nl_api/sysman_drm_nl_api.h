/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/sysman/source/shared/linux/nl_api/sysman_drm_ras_types.h"
#include "level_zero/sysman/source/shared/linux/nl_api/sysman_nl_api.h"
#include <level_zero/zes_api.h>

#include "third_party/uapi/drm-next/drm/drm_ras.h"

#include <linux/types.h>
#include <vector>

namespace L0 {
namespace Sysman {

class Operation;

class DrmNlApi {
  public:
    DrmNlApi();
    virtual ~DrmNlApi();

    MOCKABLE_VIRTUAL ze_result_t listNodes(std::vector<DrmRasNode> &nodeList);
    MOCKABLE_VIRTUAL ze_result_t getErrorCounter(const uint32_t &nodeId, const uint32_t &errorId, DrmErrorCounter &errorCounter);
    MOCKABLE_VIRTUAL ze_result_t getErrorsList(const uint32_t &nodeId, std::vector<DrmErrorCounter> &errorList);

    int handleMsg(struct nl_msg *msg);
    ze_result_t listNodesRsp(struct nl_cache_ops *ops, struct genl_cmd *cmd, struct genl_info *info);
    ze_result_t getErrorListRsp(struct nl_cache_ops *ops, struct genl_cmd *cmd, struct genl_info *info);
    ze_result_t getSingleErrorCounterRsp(struct nl_cache_ops *ops, struct genl_cmd *cmd, struct genl_info *info);

  protected:
    std::unique_ptr<NlApi> pNlApi;
    ze_result_t initConnection();
    void cleanupConnection(bool freeSocket);

  private:
    struct Operation {
        uint16_t cmdOp;
        bool done = false;
        bool parseSingleErrorCounter = false;
        void *pOutput;
        ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
        Operation(uint16_t cmdOp, void *pOutput) : cmdOp(cmdOp), pOutput(pOutput) {}
    };

    ze_result_t allocMsg(const uint16_t &cmdOp, const bool useDumpFlags, struct nl_msg *&msg);
    ze_result_t issueRequestReadOne(const uint16_t cmdOp, const uint32_t &nodeId, const uint32_t &errorId, void *pOutput);
    ze_result_t issueRequestQueryErrors(const uint16_t &cmdOp, const uint32_t &nodeId, void *pOutput);
    ze_result_t performTransaction(const uint16_t &cmdOp, struct nl_msg *msg, void *pOutput, const bool parseSingleErrorCounter);

    ze_result_t issueRequestListNodes(void *pOutput);
    void setupNlOperations();

    bool initted = false;
    struct nl_sock *nlSock = nullptr;
    int familyId = 0;

    struct genl_ops ops = {};

    struct genl_cmd newCmds[DRM_RAS_CMD_MAX] = {};
    struct nla_policy nodePolicy[DRM_RAS_A_NODE_ATTRS_MAX + 1] = {};
    struct nla_policy errorPolicy[DRM_RAS_A_ERROR_COUNTER_ATTRS_MAX + 1] = {};

    std::unique_ptr<Operation> currentOperation = nullptr;
    std::string deviceName = {};
};

} // namespace Sysman
} // namespace L0
