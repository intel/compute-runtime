/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/sysman/linux/nl_api/nl_api.h"

#include "iaf/iaf_netlink.h"

#include <linux/types.h>
#include <list>
#include <netlink/genl/genl.h>
#include <netlink/genl/mngt.h>
#include <vector>

namespace L0 {

const std::string iafPath = "device/";
const std::string iafDirectoryLegacy = "iaf.";
const std::string iafDirectory = "i915.iaf.";
const std::string fabricIdFile = "/iaf_fabric_id";

class IafNlApi;

class Operation {
  public:
    uint16_t cmdOp;
    bool done = false;
    void *pOutput;
    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    Operation(uint16_t cmdOp, void *pOutput) : cmdOp(cmdOp), pOutput(pOutput) {}
};

struct IafPortId {
    uint32_t fabricId = 0;
    uint32_t attachId = 0;
    uint8_t portNumber = 0;

    constexpr IafPortId(uint32_t fabricId, uint32_t attachId, uint32_t portNumber) : fabricId(fabricId), attachId(attachId), portNumber(portNumber) {}
    IafPortId() = default;
};

struct IafPortSpeed {
    int64_t bitRate = 0;
    int32_t width = 0;
};

struct IafPortThroughPut {
    uint64_t timestamp = 0;
    uint64_t rxCounter = 0;
    uint64_t txCounter = 0;
};

struct IafPortState {
    uint8_t healthStatus = 0;
    uint8_t lqi = 0;
    uint8_t lwd = 0;
    uint8_t rate = 0;
    uint8_t failed = 0;
    uint8_t isolated = 0;
    uint8_t flapping = 0;
    uint8_t linkDown = 0;
    uint8_t didNotTrain = 0;
};

struct IafPort {
    bool onSubdevice = false;
    IafPortId portId = {};
    std::string model = {};
    IafPortSpeed maxRxSpeed = {};
    IafPortSpeed maxTxSpeed = {};
};

class IafNlApi {
  public:
    IafNlApi();
    virtual ~IafNlApi();

    MOCKABLE_VIRTUAL ze_result_t handleResponse(const uint16_t cmdOp, struct genl_info *info, void *pOutput);
    MOCKABLE_VIRTUAL ze_result_t fPortStatusQuery(const IafPortId portId, IafPortState &state);
    MOCKABLE_VIRTUAL ze_result_t getThroughput(const IafPortId portId, IafPortThroughPut &throughput);
    MOCKABLE_VIRTUAL ze_result_t portStateQuery(const IafPortId portId, bool &enabled);
    MOCKABLE_VIRTUAL ze_result_t portBeaconStateQuery(const IafPortId portId, bool &enabled);
    MOCKABLE_VIRTUAL ze_result_t portBeaconEnable(const IafPortId portId);
    MOCKABLE_VIRTUAL ze_result_t portBeaconDisable(const IafPortId portId);
    MOCKABLE_VIRTUAL ze_result_t portEnable(const IafPortId portId);
    MOCKABLE_VIRTUAL ze_result_t portDisable(const IafPortId portId);
    MOCKABLE_VIRTUAL ze_result_t portUsageEnable(const IafPortId portId);
    MOCKABLE_VIRTUAL ze_result_t portUsageDisable(const IafPortId portId);
    MOCKABLE_VIRTUAL ze_result_t remRequest();
    MOCKABLE_VIRTUAL ze_result_t routingGenQuery(uint32_t &start, uint32_t &end);
    MOCKABLE_VIRTUAL ze_result_t deviceEnum(std::vector<uint32_t> &fabricIds);
    MOCKABLE_VIRTUAL ze_result_t fabricDeviceProperties(const uint32_t fabricId, uint32_t &numSubdevices);
    MOCKABLE_VIRTUAL ze_result_t subdevicePropertiesGet(const uint32_t fabricId, const uint32_t attachId, uint64_t &guid, std::vector<uint8_t> &ports);
    MOCKABLE_VIRTUAL ze_result_t fportProperties(const IafPortId portId, uint64_t &neighborGuid, uint8_t &neighborPortNumber,
                                                 IafPortSpeed &maxRxSpeed, IafPortSpeed &maxTxSpeed,
                                                 IafPortSpeed &rxSpeed, IafPortSpeed &txSpeed);
    MOCKABLE_VIRTUAL ze_result_t getPorts(const std::string &devicePciPath, std::vector<IafPort> &ports);
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
    ze_result_t initPorts(const uint32_t fabricId, std::vector<IafPort> &iafPorts);
    int32_t translateWidth(uint8_t width);

    bool initted = false;
    struct nl_sock *nlSock = nullptr;
    int familyId = 0;

    struct nla_policy policy[_IAF_ATTR_COUNT] = {};
    struct genl_cmd cmds[_IAF_CMD_OP_COUNT] = {};
    struct genl_ops ops = {};
};

} // namespace L0
