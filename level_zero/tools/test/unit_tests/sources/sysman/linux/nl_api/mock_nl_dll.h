/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/sysman/linux/nl_api/nl_api.h"

#include "gmock/gmock.h"

// Define opaque types so variables can be allocated
struct nl_sock {
};

struct nl_msg {
};

namespace L0 {
namespace ult {

class MockNlDll : public NEO::OsLibrary {
  public:
    bool isLoaded() override { return false; }
    void *getProcAddress(const std::string &procName) override;

    void deleteEntryPoint(const std::string &procName);

    MockNlDll();

    static struct nl_sock mockNlSock;
    static struct nl_msg mockNlMsg;
    static struct nlmsghdr mockNlmsghdr;
    static struct nlattr mockNlattr;
    static struct nlattr mockNextNlattr;
    static struct genl_ops mockGenlOps;
    static nl_recvmsg_msg_cb_t mockCb;

    constexpr static int mockFamilyId = 0x2020;
    constexpr static char mockFamilyName[] = "TestName";
    constexpr static void *mockArgP = nullptr;
    constexpr static uint32_t mockPort = NL_AUTO_PID;
    constexpr static uint32_t mockSeq = NL_AUTO_SEQ;
    constexpr static int mockHdrlen = NLMSG_HDRLEN;
    constexpr static int mockFlags = 0;
    constexpr static uint8_t mockCmd = 1;
    constexpr static uint8_t mockVersion = 2;
    constexpr static int mockType = 3;
    constexpr static uint8_t mockU8Val = 0x7fU;
    constexpr static uint16_t mockU16Val = 0x7fffU;
    constexpr static uint32_t mockU32Val = 0x7fffffffU;
    constexpr static uint64_t mockU64Val = 0x7fffffffffffffffUL;
    constexpr static int mockAttr = 4;
    constexpr static int mockAttrLen = 8;
    constexpr static int mockRemainBefore = 20;
    constexpr static int mockRemainAfter = 16;
    constexpr static enum nl_cb_type mockCbType = NL_CB_VALID;
    constexpr static enum nl_cb_kind mockCbKind = NL_CB_CUSTOM;

  private:
    std::map<std::string, void *> funcMap;
};

} // namespace ult
} // namespace L0
