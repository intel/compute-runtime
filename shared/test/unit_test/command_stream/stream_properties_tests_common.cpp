/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/command_stream/stream_properties_tests_common.h"

#include "shared/source/command_stream/stream_properties.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "test.h"

using namespace NEO;

TEST(StreamPropertiesTests, whenPropertyValueIsChangedThenProperStateIsSet) {
    NEO::StreamProperty streamProperty;

    EXPECT_EQ(-1, streamProperty.value);
    EXPECT_FALSE(streamProperty.isDirty);

    streamProperty.set(-1);
    EXPECT_EQ(-1, streamProperty.value);
    EXPECT_FALSE(streamProperty.isDirty);

    int32_t valuesToTest[] = {0, 1};
    for (auto valueToTest : valuesToTest) {
        streamProperty.set(valueToTest);
        EXPECT_EQ(valueToTest, streamProperty.value);
        EXPECT_TRUE(streamProperty.isDirty);

        streamProperty.isDirty = false;
        streamProperty.set(valueToTest);
        EXPECT_EQ(valueToTest, streamProperty.value);
        EXPECT_FALSE(streamProperty.isDirty);

        streamProperty.set(-1);
        EXPECT_EQ(valueToTest, streamProperty.value);
        EXPECT_FALSE(streamProperty.isDirty);
    }
}

TEST(StreamPropertiesTests, whenSettingStateComputeModePropertiesThenCorrectValuesAreSet) {
    StreamProperties properties;
    for (auto requiresCoherency : ::testing::Bool()) {
        properties.setStateComputeModeProperties(requiresCoherency, 0u, false, false, false);
        EXPECT_EQ(requiresCoherency, properties.stateComputeMode.isCoherencyRequired.value);
    }
}

TEST(StreamPropertiesTests, givenVariousStatesOfThePropertiesWhenIsStateComputeModeDirtyIsQueriedThenCorrectValueIsReturned) {
    struct MockStateComputeModeProperties : StateComputeModeProperties {
        using StateComputeModeProperties::clearIsDirty;
    };
    MockStateComputeModeProperties properties;

    EXPECT_FALSE(properties.isDirty());
    for (auto pProperty : getAllStateComputeModeProperties(properties)) {
        pProperty->isDirty = true;
        EXPECT_TRUE(properties.isDirty());
        pProperty->isDirty = false;
        EXPECT_FALSE(properties.isDirty());
    }
    for (auto pProperty : getAllStateComputeModeProperties(properties)) {
        pProperty->isDirty = true;
    }
    EXPECT_TRUE(properties.isDirty());

    properties.clearIsDirty();
    for (auto pProperty : getAllStateComputeModeProperties(properties)) {
        EXPECT_FALSE(pProperty->isDirty);
    }
    EXPECT_FALSE(properties.isDirty());
}
