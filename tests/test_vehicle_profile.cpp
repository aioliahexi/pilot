#include <gtest/gtest.h>
#include "control/vehicle_profile.h"
#include "control/vehicle_controller.h"
#include <stdexcept>

using namespace pilot::control;

// ---- VehicleProfile factory methods ----------------------------------------

TEST(VehicleProfileTest, EcoValues) {
    auto p = VehicleProfile::eco();
    EXPECT_EQ(p.mode,             VehicleMode::ECO);
    EXPECT_DOUBLE_EQ(p.max_rpm,   3000.0);
    EXPECT_DOUBLE_EQ(p.target_soc, 80.0);
    EXPECT_DOUBLE_EQ(p.temp_threshold, 80.0);
    EXPECT_DOUBLE_EQ(p.regen_brake_level, 0.8);
    EXPECT_DOUBLE_EQ(p.power_limit, 50.0);
}

TEST(VehicleProfileTest, NormalValues) {
    auto p = VehicleProfile::normal();
    EXPECT_EQ(p.mode, VehicleMode::NORMAL);
    EXPECT_DOUBLE_EQ(p.max_rpm,    5000.0);
    EXPECT_DOUBLE_EQ(p.power_limit, 100.0);
}

TEST(VehicleProfileTest, SportValues) {
    auto p = VehicleProfile::sport();
    EXPECT_EQ(p.mode, VehicleMode::SPORT);
    EXPECT_DOUBLE_EQ(p.max_rpm,    8000.0);
    EXPECT_DOUBLE_EQ(p.power_limit, 200.0);
    EXPECT_DOUBLE_EQ(p.regen_brake_level, 0.2);
}

TEST(VehicleProfileTest, ChargeValues) {
    auto p = VehicleProfile::charge();
    EXPECT_EQ(p.mode, VehicleMode::CHARGE);
    EXPECT_DOUBLE_EQ(p.target_soc,  100.0);
    EXPECT_DOUBLE_EQ(p.regen_brake_level, 1.0);
    EXPECT_DOUBLE_EQ(p.power_limit,  20.0);
}

// ---- mode_to_string / mode_from_string round trip --------------------------

TEST(VehicleProfileTest, ModeToStringRoundTrip) {
    for (auto mode : {VehicleMode::ECO, VehicleMode::NORMAL,
                      VehicleMode::SPORT, VehicleMode::CHARGE}) {
        EXPECT_EQ(mode_from_string(mode_to_string(mode)), mode);
    }
}

TEST(VehicleProfileTest, ModeFromStringKnownValues) {
    EXPECT_EQ(mode_from_string("ECO"),    VehicleMode::ECO);
    EXPECT_EQ(mode_from_string("NORMAL"), VehicleMode::NORMAL);
    EXPECT_EQ(mode_from_string("SPORT"),  VehicleMode::SPORT);
    EXPECT_EQ(mode_from_string("CHARGE"), VehicleMode::CHARGE);
}

TEST(VehicleProfileTest, ModeFromStringThrowsOnUnknown) {
    EXPECT_THROW(mode_from_string("TURBO"), std::invalid_argument);
}

// ---- VehicleController -----------------------------------------------------

TEST(VehicleControllerTest, DefaultModeIsNormal) {
    VehicleController ctrl;
    EXPECT_EQ(ctrl.current_mode(), VehicleMode::NORMAL);
}

TEST(VehicleControllerTest, SetProfile) {
    VehicleController ctrl;
    ctrl.set_profile(VehicleProfile::sport());
    EXPECT_EQ(ctrl.current_mode(), VehicleMode::SPORT);
    EXPECT_DOUBLE_EQ(ctrl.current_profile().max_rpm, 8000.0);
}

TEST(VehicleControllerTest, SetProfileByMode) {
    VehicleController ctrl;
    ctrl.set_profile_by_mode(VehicleMode::ECO);
    EXPECT_EQ(ctrl.current_mode(), VehicleMode::ECO);

    ctrl.set_profile_by_mode(VehicleMode::CHARGE);
    EXPECT_EQ(ctrl.current_mode(), VehicleMode::CHARGE);
}

TEST(VehicleControllerTest, StatusJsonContainsMode) {
    VehicleController ctrl;
    ctrl.set_profile(VehicleProfile::sport());
    std::string json = ctrl.status_json();
    EXPECT_NE(json.find("SPORT"), std::string::npos);
    EXPECT_NE(json.find("max_rpm"), std::string::npos);
}

TEST(VehicleControllerTest, RegisterWithEngineDoesNotThrow) {
    pilot::engine::ConditionEngine engine(std::chrono::seconds(10));
    VehicleController ctrl;
    EXPECT_NO_THROW(ctrl.register_with_engine(engine));
}
