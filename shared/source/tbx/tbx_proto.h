/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {

enum HasMsgType {
    HAS_MMIO_REQ_TYPE = 0,
    HAS_MMIO_RES_TYPE = 1,
    HAS_GTT_REQ_TYPE = 2,
    HAS_GTT_RES_TYPE = 3,
    HAS_WRITE_DATA_REQ_TYPE = 4,
    HAS_READ_DATA_REQ_TYPE = 5,
    HAS_READ_DATA_RES_TYPE = 6,
    HAS_MARKER_REQ_TYPE = 7,
    HAS_MARKER_RES_TYPE = 8,
    HAS_REPORT_REND_END_REQ_TYPE = 9,
    HAS_REPORT_REND_END_RES_TYPE = 10,
    HAS_CONTROL_REQ_TYPE = 11,
    HAS_PARAMS_REQ_TYPE = 12,
    HAS_PARAMS_RES_TYPE = 13,
    HAS_PCICFG_REQ_TYPE = 14,
    HAS_PCICFG_RES_TYPE = 15,
    HAS_GTT_PARAMS_REQ_TYPE = 16,
    HAS_EVENT_REQ_TYPE = 17,
    HAS_INNER_VAR_REQ_TYPE = 18,
    HAS_INNER_VAR_RES_TYPE = 19,
    HAS_INNER_VAR_LIST_REQ_TYPE = 20,
    HAS_INNER_VAR_LIST_RES_TYPE = 21,
    HAS_FUNNY_IO_REQ_TYPE = 22,
    HAS_FUNNY_IO_RES_TYPE = 23,
    HAS_IO_REQ_TYPE = 24,
    HAS_IO_RES_TYPE = 25,
    HAS_RPC_REQ_TYPE = 26,
    HAS_RPC_RES_TYPE = 27,
    HAS_CL_FLUSH_REQ_TYPE = 28,
    HAS_CL_FLUSH_RES_TYPE = 29,
    HAS_SYNC_ALL_PAGES_REQ_TYPE = 30,
    HAS_SYNC_ALL_PAGES_RES_TYPE = 31,
    HAS_GD2_MESSAGE_TYPE = 32,
    HAS_SIMTIME_RES_TYPE = 33,
    HAS_RL_STATUS_RES_TYPE = 34,
    NUM_OF_MSG_TYPE
};

struct HasHdr {
    union {
        uint32_t msgType;
        HasMsgType type;
    };
    uint32_t transID;
    uint32_t size;
};

enum {
    MSG_TYPE_MMIO = 0,
    MSG_TYPE_IO,
    MSG_TYPE_FUNNY_IO
};

struct HasMmioReq {
    uint32_t write : 1;
    uint32_t size : 3;
    uint32_t devIdx : 2;
    uint32_t msgType : 3;
    uint32_t reserved : 7;
    uint32_t delay : 16;

    uint32_t offset;
    uint32_t data;

    enum {
        HasMsgType = HAS_MMIO_REQ_TYPE
    };
};

struct HasMmioExtReq {
    struct HasMmioReq mmioReq;
    uint32_t sourceid : 8;
    uint32_t reserved1 : 24;
    enum {
        HasMsgType = HAS_MMIO_REQ_TYPE
    };
};

struct HasMmioRes {
    uint32_t data;

    enum {
        HasMsgType = HAS_MMIO_RES_TYPE
    };
};

struct HasGtt32Req {
    uint32_t write : 1;
    uint32_t reserved : 31;

    uint32_t offset;
    uint32_t data;

    enum {
        HasMsgType = HAS_GTT_REQ_TYPE
    };
};

struct HasGtt32Res {
    uint32_t data;

    enum {
        HasMsgType = HAS_GTT_RES_TYPE
    };
};

struct HasGtt64Req {
    uint32_t write : 1;
    uint32_t reserved : 31;

    uint32_t offset;
    uint32_t data;
    uint32_t dataH;

    enum {
        HasMsgType = HAS_GTT_REQ_TYPE
    };
};

struct HasGtt64Res {
    uint32_t data;
    uint32_t dataH;

    enum {
        HasMsgType = HAS_GTT_RES_TYPE
    };
};

struct HasWriteDataReq {
    uint32_t addrType : 1;
    uint32_t maskExist : 1;
    uint32_t frontdoor : 1;
    uint32_t takeOwnership : 1;
    uint32_t modelOwned : 1;
    uint32_t cachelineDisable : 1;
    uint32_t memoryType : 2;
    uint32_t reserved : 16;
    uint32_t addressH : 8;
    uint32_t address;
    uint32_t size;

    enum {
        HasMsgType = HAS_WRITE_DATA_REQ_TYPE
    };
};

struct HasReadDataReq {
    uint32_t addrType : 1;
    uint32_t frontdoor : 1;
    uint32_t ownershipReq : 1;
    uint32_t modelOwned : 1;
    uint32_t cachelineDisable : 1;
    uint32_t reserved : 19;
    uint32_t addressH : 8;
    uint32_t address;
    uint32_t size;

    enum {
        HasMsgType = HAS_READ_DATA_REQ_TYPE
    };
};

struct HasReadDataRes {
    uint32_t addrType : 1;
    uint32_t maskExist : 1;
    uint32_t lastPage : 1;
    uint32_t ownershipRes : 1;
    uint32_t reserved : 20;
    uint32_t addressH : 8;
    uint32_t address;
    uint32_t size;

    enum {
        HasMsgType = HAS_READ_DATA_RES_TYPE
    };
};

struct HasControlReq {
    uint32_t reset : 1;         // [0:0]
    uint32_t has : 1;           // [1:1]
    uint32_t rdOnDemand : 1;    // [2:2]
    uint32_t writeMask : 1;     // [3:3]
    uint32_t timeAdv : 1;       // [4:4]
    uint32_t asyncMsg : 1;      // [5:5]
    uint32_t quit : 1;          // [6:6]
    uint32_t cncryEnb : 1;      // [7:7]
    uint32_t stimeEnb : 1;      // [8:8]
    uint32_t fullReset : 1;     // [9:9]
    uint32_t autoOwnership : 1; // [10:10]
    uint32_t backdoorModel : 1; // [11:11]
    uint32_t flush : 1;         // [12:12]
    uint32_t reserved : 3;      // [15:13]

    uint32_t resetMask : 1;         // [16:16]
    uint32_t hasMask : 1;           // [17:17]
    uint32_t rdOnDemandMask : 1;    // [18:18]
    uint32_t writeMaskMask : 1;     // [19:19]
    uint32_t timeAdvMask : 1;       // [20:20]
    uint32_t asyncMsgMask : 1;      // [21:21]
    uint32_t quitMask : 1;          // [22:22]
    uint32_t cncryEnbMask : 1;      // [23:23]
    uint32_t stimeEnbMask : 1;      // [24:24]
    uint32_t fullResetMask : 1;     // [25:25]
    uint32_t autoOwnershipMask : 1; // [26:26]
    uint32_t backdoorModelMask : 1; // [27:27]
    uint32_t flushMask : 1;         // [28:28]
    uint32_t reservedMask : 3;      // [31:29]

    enum {
        HasMsgType = HAS_CONTROL_REQ_TYPE
    };
};

struct HasReportRendEndReq {
    uint32_t timeout;

    enum {
        HasMsgType = HAS_REPORT_REND_END_REQ_TYPE
    };
};

struct HasReportRendEndRes {
    uint32_t timeout : 1;
    uint32_t reserved : 31;

    enum {
        HasMsgType = HAS_REPORT_REND_END_RES_TYPE
    };
};

struct HasPcicfgReq {
    uint32_t write : 1;
    uint32_t size : 3;
    uint32_t bus : 8;
    uint32_t device : 5;
    uint32_t function : 3;
    uint32_t reserved : 12;
    uint32_t offset;
    uint32_t data;

    enum {
        HasMsgType = HAS_PCICFG_REQ_TYPE
    };
};

struct HasPcicfgRes {
    uint32_t data;
};

struct HasGttParamsReq {
    uint32_t base;
    uint32_t baseH : 8;
    uint32_t size : 24;

    enum {
        HasMsgType = HAS_GTT_PARAMS_REQ_TYPE
    };
};

struct HasEventObsoleteReq {
    uint32_t offset;
    uint32_t data;

    enum {
        HasMsgType = HAS_EVENT_REQ_TYPE
    };
};

struct HasEventReq {
    uint32_t offset;
    uint32_t data;
    uint32_t devIdx : 2;
    uint32_t reserved : 30;

    enum {
        HasMsgType = HAS_EVENT_REQ_TYPE
    };
};

struct HasInnerVarReq {
    uint32_t write : 1;
    uint32_t nonDword : 16;
    uint32_t reserved : 15;
    uint32_t id;
    uint32_t data;

    enum {
        HasMsgType = HAS_INNER_VAR_REQ_TYPE
    };
};

struct HasInnerVarRes {
    uint32_t data;

    enum {
        HasMsgType = HAS_INNER_VAR_RES_TYPE
    };
};

struct HasInnerVarListRes {
    uint32_t size;

    enum {
        HasMsgType = HAS_INNER_VAR_LIST_RES_TYPE
    };
};

struct HasInternalVarListEntryRes {
    uint32_t id;
    uint32_t min;
    uint32_t max;
    uint32_t descSize;
};

struct HasFunnyIoReq {
    uint32_t write : 1;
    uint32_t reserved : 28;
    uint32_t size : 3;
    uint32_t offset;
    uint32_t value;

    enum {
        HasMsgType = HAS_FUNNY_IO_REQ_TYPE
    };
};

struct HasFunnyIoRes {
    uint32_t data;

    enum {
        HasMsgType = HAS_FUNNY_IO_RES_TYPE
    };
};

struct HasIoReq {
    uint32_t write : 1;
    uint32_t devIdx : 2;
    uint32_t reserved : 26;
    uint32_t size : 3;
    uint32_t offset;
    uint32_t value;

    enum {
        HasMsgType = HAS_IO_REQ_TYPE
    };
};

struct HasIoRes {
    uint32_t data;

    enum {
        HasMsgType = HAS_IO_RES_TYPE
    };
};

struct HasRpcReq {
    uint32_t size;

    enum {
        HasMsgType = HAS_RPC_REQ_TYPE
    };
};

struct HasRpcRes {
    uint32_t status;
    uint32_t size;

    enum {
        HasMsgType = HAS_RPC_RES_TYPE
    };
};

struct HasClFlushReq {
    uint32_t reserved : 23;
    uint32_t ignore : 1;
    uint32_t addressH : 8;
    uint32_t address;
    uint32_t size;
    uint32_t delay;

    enum {
        HasMsgType = HAS_CL_FLUSH_REQ_TYPE
    };
};

struct HasClFlushRes {
    uint32_t data;

    enum {
        HasMsgType = HAS_CL_FLUSH_RES_TYPE
    };
};

struct HasSimtimeRes {
    uint32_t dataL;
    uint32_t dataH;

    enum {
        HasMsgType = HAS_SIMTIME_RES_TYPE
    };
};

struct HasGd2Message {
    uint32_t subOpcode;
    uint32_t data[1];

    enum {
        HasMsgType = HAS_GD2_MESSAGE_TYPE
    };
};

union HasMsgBody {
    struct HasMmioReq mmioReq;
    struct HasMmioExtReq mmioReqExt;
    struct HasMmioRes mmioRes;
    struct HasGtt32Req gtt32Req;
    struct HasGtt32Res gtt32Res;
    struct HasGtt64Req gtt64Req;
    struct HasGtt64Res gtt64Res;
    struct HasWriteDataReq writeReq;
    struct HasReadDataReq readReq;
    struct HasReadDataRes readRes;
    struct HasControlReq controlReq;
    struct HasReportRendEndReq renderReq;
    struct HasReportRendEndRes renderRes;
    struct HasPcicfgReq pcicfgReq;
    struct HasPcicfgRes pcicfgRes;
    struct HasGttParamsReq gttParamsReq;
    struct HasEventReq eventReq;
    struct HasEventObsoleteReq eventObsoleteReq;
    struct HasInnerVarReq innerVarReq;
    struct HasInnerVarRes innerVarRes;
    struct HasInnerVarListRes innerVarListRes;
    struct HasIoReq ioReq;
    struct HasIoRes ioRes;
    struct HasRpcReq rpcReq;
    struct HasRpcRes rpcRes;
    struct HasClFlushReq flushReq;
    struct HasClFlushRes flushRes;
    struct HasSimtimeRes stimeRes;
    struct HasGd2Message gd2MessageReq;
};

struct HasMsg {
    struct HasHdr hdr;
    union HasMsgBody u;
};

enum MemType : uint32_t {
    system = 0,
    local = 1,
};

} // namespace NEO
