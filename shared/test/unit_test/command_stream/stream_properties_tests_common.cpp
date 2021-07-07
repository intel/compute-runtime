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
        for (auto largeGrf : ::testing::Bool()) {
            properties.stateComputeMode.setProperties(requiresCoherency, largeGrf ? 256 : 128, 0u);
            EXPECT_EQ(largeGrf, properties.stateComputeMode.largeGrfMode.value);
            EXPECT_EQ(requiresCoherency, properties.stateComputeMode.isCoherencyRequired.value);
        }
    }
}

template <typename PropertiesT>
using getAllPropertiesFunctionPtr = std::vector<StreamProperty *> (*)(PropertiesT &properties);

template <typename PropertiesT, getAllPropertiesFunctionPtr<PropertiesT> getAllProperties>
void verifyIsDirty() {
    struct MockPropertiesT : PropertiesT {
        using PropertiesT::clearIsDirty;
    };
    MockPropertiesT properties;
    auto allProperties = getAllProperties(properties);

    EXPECT_FALSE(properties.isDirty());
    for (auto pProperty : allProperties) {
        pProperty->isDirty = true;
        EXPECT_TRUE(properties.isDirty());
        pProperty->isDirty = false;
        EXPECT_FALSE(properties.isDirty());
    }
    for (auto pProperty : allProperties) {
        pProperty->isDirty = true;
    }

    EXPECT_EQ(!allProperties.empty(), properties.isDirty());

    properties.clearIsDirty();
    for (auto pProperty : allProperties) {
        EXPECT_FALSE(pProperty->isDirty);
    }
    EXPECT_FALSE(properties.isDirty());
}

TEST(StreamPropertiesTests, givenVariousStatesOfStateComputeModePropertiesWhenIsDirtyIsQueriedThenCorrectValueIsReturned) {
    verifyIsDirty<StateComputeModeProperties, &getAllStateComputeModeProperties>();
}

TEST(StreamPropertiesTests, givenVariousStatesOfFrontEndPropertiesWhenIsDirtyIsQueriedThenCorrectValueIsReturned) {
    verifyIsDirty<FrontEndProperties, getAllFrontEndProperties>();
}

template <typename PropertiesT, getAllPropertiesFunctionPtr<PropertiesT> getAllProperties>
void verifySettingPropertiesFromOtherStruct() {
    PropertiesT propertiesDestination;
    PropertiesT propertiesSource;

    auto allPropertiesDestination = getAllProperties(propertiesDestination);
    auto allPropertiesSource = getAllProperties(propertiesSource);

    int valueToSet = 1;
    for (auto pProperty : allPropertiesSource) {
        pProperty->value = valueToSet;
        valueToSet++;
    }
    EXPECT_FALSE(propertiesSource.isDirty());

    propertiesDestination.setProperties(propertiesSource);
    EXPECT_EQ(!allPropertiesDestination.empty(), propertiesDestination.isDirty());

    int expectedValue = 1;
    for (auto pProperty : allPropertiesDestination) {
        EXPECT_EQ(expectedValue, pProperty->value);
        EXPECT_TRUE(pProperty->isDirty);
        expectedValue++;
    }

    propertiesDestination.setProperties(propertiesSource);
    EXPECT_FALSE(propertiesDestination.isDirty());

    expectedValue = 1;
    for (auto pProperty : allPropertiesDestination) {
        EXPECT_EQ(expectedValue, pProperty->value);
        EXPECT_FALSE(pProperty->isDirty);
        expectedValue++;
    }
}

TEST(StreamPropertiesTests, givenOtherStateComputeModePropertiesStructWhenSetPropertiesIsCalledThenCorrectValuesAreSet) {
    verifySettingPropertiesFromOtherStruct<StateComputeModeProperties, &getAllStateComputeModeProperties>();
}

TEST(StreamPropertiesTests, givenOtherFrontEndPropertiesStructWhenSetPropertiesIsCalledThenCorrectValuesAreSet) {
    verifySettingPropertiesFromOtherStruct<FrontEndProperties, getAllFrontEndProperties>();
}
