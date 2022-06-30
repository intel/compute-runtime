/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/sysman/linux/nl_api/nl_api.h"

#include "iaf/iaf_netlink.h"
#include "sysman/linux/os_sysman_imp.h"

#include <linux/types.h>
#include <list>
#include <netlink/genl/genl.h>
#include <netlink/genl/mngt.h>
#include <vector>

namespace L0 {

class IafNlApi;

class Operation {
  public:
    uint16_t cmdOp;
    bool done = false;
    void *pOutput;
    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    Operation(uint16_t cmdOp, void *pOutput) : cmdOp(cmdOp), pOutput(pOutput) {}
};

class IafNlApi {
  public:
    IafNlApi();
    virtual ~IafNlApi();

    MOCKABLE_VIRTUAL ze_result_t handleResponse(const uint16_t cmdOp, struct genl_info *info, void *pOutput);
    MOCKABLE_VIRTUAL ze_result_t fPortStatusQuery(const zes_fabric_port_id_t portId, zes_fabric_port_state_t &state);
    MOCKABLE_VIRTUAL ze_result_t getThroughput(const zes_fabric_port_id_t portId, zes_fabric_port_throughput_t &throughput);
    MOCKABLE_VIRTUAL ze_result_t portStateQuery(const zes_fabric_port_id_t portId, bool &enabled);
    MOCKABLE_VIRTUAL ze_result_t portBeaconStateQuery(const zes_fabric_port_id_t portId, bool &enabled);
    MOCKABLE_VIRTUAL ze_result_t portBeaconEnable(const zes_fabric_port_id_t portId);
    MOCKABLE_VIRTUAL ze_result_t portBeaconDisable(const zes_fabric_port_id_t portId);
    MOCKABLE_VIRTUAL ze_result_t portEnable(const zes_fabric_port_id_t portId);
    MOCKABLE_VIRTUAL ze_result_t portDisable(const zes_fabric_port_id_t portId);
    MOCKABLE_VIRTUAL ze_result_t portUsageEnable(const zes_fabric_port_id_t portId);
    MOCKABLE_VIRTUAL ze_result_t portUsageDisable(const zes_fabric_port_id_t portId);
    MOCKABLE_VIRTUAL ze_result_t remRequest();
    MOCKABLE_VIRTUAL ze_result_t routingGenQuery(uint32_t &start, uint32_t &end);
    MOCKABLE_VIRTUAL ze_result_t deviceEnum(std::vector<uint32_t> &fabricIds);
    MOCKABLE_VIRTUAL ze_result_t fabricDeviceProperties(const uint32_t fabricId, uint32_t &numSubdevices);
    MOCKABLE_VIRTUAL ze_result_t subdevicePropertiesGet(const uint32_t fabricId, const uint32_t attachId, uint64_t &guid, std::vector<uint8_t> &ports);
    MOCKABLE_VIRTUAL ze_result_t fportProperties(const zes_fabric_port_id_t portId, uint64_t &neighborGuid, uint8_t &neighborPortNumber,
                                                 zes_fabric_port_speed_t &maxRxSpeed, zes_fabric_port_speed_t &maxTxSpeed,
                                                 zes_fabric_port_speed_t &rxSpeed, zes_fabric_port_speed_t &txSpeed);

    std::list<uint64_t> validContexts = {};

    int handleMsg(struct nl_msg *msg);
    int nlOperation(struct nl_cache_ops *ops, struct genl_cmd *cmd, struct genl_info *info);

  protected:
    std::unique_ptr<NlApi> pNlApi;
    ze_result_t init();
    void cleanup();

  private:
    ze_result_t allocMsg(const uint16_t cmdOp, struct nl_msg *&msg);
    ze_result_t issueRequest(const uint16_t cmdOp, const uint32_t fabricId, const uint32_t attachId, const uint8_t portNumber, void *pOutput);
    ze_result_t issueRequest(const uint16_t cmdOp, const uint32_t fabricId, const uint32_t attachId, void *pOutput);
    ze_result_t issueRequest(const uint16_t cmdOp, const uint32_t fabricId, void *pOutput);
    ze_result_t issueRequest(const uint16_t cmdOp, void *pOutput);
    ze_result_t performTransaction(const uint16_t cmdOp, struct nl_msg *msg, void *pOutput);
    ze_result_t fPortStatusQueryRsp(struct genl_info *info, void *pOutput);
    ze_result_t getThroughputRsp(struct genl_info *info, void *pOutput);
    ze_result_t portStateQueryRsp(struct genl_info *info, void *pOutput);
    ze_result_t portBeaconStateQueryRsp(struct genl_info *info, void *pOutput);
    ze_result_t routingGenQueryRsp(struct genl_info *info, void *pOutput);
    ze_result_t deviceEnumRsp(struct genl_info *info, void *pOutput);
    ze_result_t fabricDevicePropertiesRsp(struct genl_info *info, void *pOutput);
    ze_result_t subdevicePropertiesGetRsp(struct genl_info *info, void *pOutput);
    ze_result_t fportPropertiesRsp(struct genl_info *info, void *pOutput);
    int32_t translateWidth(uint8_t width);

    bool initted = false;
    struct nl_sock *nlSock = nullptr;
    int familyId = 0;

    struct nla_policy policy[_IAF_ATTR_COUNT] = {};
    struct genl_cmd cmds[_IAF_CMD_OP_COUNT] = {};
    struct genl_ops ops = {};
};

} // namespace L0
