#include <gtest/gtest.h>
#include "engine/condition_engine.h"
#include "sensors/sensor_base.h"
#include <chrono>
#include <vector>
#include <string>

using namespace pilot::engine;
using pilot::sensors::SensorReading;

static SensorReading make_r(const std::string& id, double value) {
    return SensorReading{std::chrono::system_clock::now(), value, "unit", id};
}

TEST(ConditionEngineTest, ThresholdAboveTriggered) {
    ConditionEngine engine(std::chrono::seconds(10));
    engine.add_condition(Condition{"hot", "temp", ConditionType::THRESHOLD_ABOVE, 100.0});

    std::vector<std::pair<std::string, bool>> events;
    engine.add_callback([&](const std::string& name, const SensorReading&, bool triggered) {
        events.push_back({name, triggered});
    });

    engine.process(make_r("temp", 105.0));
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].first,  "hot");
    EXPECT_TRUE(events[0].second);
}

TEST(ConditionEngineTest, BelowThresholdNoCallback) {
    ConditionEngine engine(std::chrono::seconds(10));
    engine.add_condition(Condition{"hot", "temp", ConditionType::THRESHOLD_ABOVE, 100.0});

    int cb_count = 0;
    engine.add_callback([&](const std::string&, const SensorReading&, bool) { ++cb_count; });

    engine.process(make_r("temp", 80.0));
    EXPECT_EQ(cb_count, 0);
}

TEST(ConditionEngineTest, StateChangeTriggeredToFalse) {
    ConditionEngine engine(std::chrono::seconds(10));
    engine.add_condition(Condition{"hot", "temp", ConditionType::THRESHOLD_ABOVE, 100.0});

    std::vector<bool> states;
    engine.add_callback([&](const std::string&, const SensorReading&, bool triggered) {
        states.push_back(triggered);
    });

    engine.process(make_r("temp", 110.0)); // triggers
    engine.process(make_r("temp", 110.0)); // same state – no callback
    engine.process(make_r("temp",  90.0)); // drops back – fires callback(false)

    ASSERT_EQ(states.size(), 2u);
    EXPECT_TRUE(states[0]);
    EXPECT_FALSE(states[1]);
}

TEST(ConditionEngineTest, ThresholdBelow) {
    ConditionEngine engine(std::chrono::seconds(10));
    engine.add_condition(Condition{"cold", "temp", ConditionType::THRESHOLD_BELOW, 0.0});

    bool triggered = false;
    engine.add_callback([&](const std::string&, const SensorReading&, bool t) { triggered = t; });

    engine.process(make_r("temp", -5.0));
    EXPECT_TRUE(triggered);
}

TEST(ConditionEngineTest, MultipleConditionsSameSensor) {
    ConditionEngine engine(std::chrono::seconds(10));
    engine.add_condition(Condition{"above_50", "rpm", ConditionType::THRESHOLD_ABOVE, 50.0});
    engine.add_condition(Condition{"above_80", "rpm", ConditionType::THRESHOLD_ABOVE, 80.0});

    std::vector<std::string> fired;
    engine.add_callback([&](const std::string& name, const SensorReading&, bool t) {
        if (t) fired.push_back(name);
    });

    engine.process(make_r("rpm", 60.0));
    ASSERT_EQ(fired.size(), 1u);
    EXPECT_EQ(fired[0], "above_50");

    engine.process(make_r("rpm", 90.0));
    ASSERT_EQ(fired.size(), 2u);
    EXPECT_EQ(fired[1], "above_80");
}

TEST(ConditionEngineTest, AvgAbove) {
    ConditionEngine engine(std::chrono::seconds(60));
    engine.add_condition(Condition{"avg_high", "s", ConditionType::AVG_ABOVE, 50.0});

    bool triggered = false;
    engine.add_callback([&](const std::string&, const SensorReading&, bool t) { triggered = t; });

    // Push values with low average – should not trigger
    for (int i = 0; i < 5; ++i) engine.process(make_r("s", 20.0));
    EXPECT_FALSE(triggered);

    // Push values to push average above 50
    for (int i = 0; i < 20; ++i) engine.process(make_r("s", 100.0));
    EXPECT_TRUE(triggered);
}

TEST(ConditionEngineTest, GetStatsReturnsNulloptForUnknownSensor) {
    ConditionEngine engine(std::chrono::seconds(10));
    EXPECT_FALSE(engine.get_stats("unknown").has_value());
}

TEST(ConditionEngineTest, GetStats) {
    ConditionEngine engine(std::chrono::seconds(60));
    engine.process(make_r("s", 10.0));
    engine.process(make_r("s", 20.0));
    auto s = engine.get_stats("s");
    ASSERT_TRUE(s.has_value());
    EXPECT_DOUBLE_EQ(s->min_val, 10.0);
    EXPECT_DOUBLE_EQ(s->max_val, 20.0);
    EXPECT_DOUBLE_EQ(s->avg_val, 15.0);
}

TEST(ConditionEngineTest, RateOfChangeAbove) {
    ConditionEngine engine(std::chrono::seconds(60));
    engine.add_condition(Condition{"spike", "s", ConditionType::RATE_OF_CHANGE_ABOVE, 50.0});

    bool triggered = false;
    engine.add_callback([&](const std::string&, const SensorReading&, bool t) {
        if (t) triggered = true;
    });

    // Slow rise – should not trigger
    auto t0 = std::chrono::system_clock::now();
    SensorReading r1{t0,                                        0.0,  "u", "s"};
    SensorReading r2{t0 + std::chrono::seconds(1),             10.0, "u", "s"};
    engine.process(r1);
    engine.process(r2);
    EXPECT_FALSE(triggered);

    // Fast rise – rate = 200 / 1s = 200 > 50
    SensorReading r3{t0 + std::chrono::seconds(2),             10.0, "u", "s"};
    SensorReading r4{t0 + std::chrono::milliseconds(2100),    210.0, "u", "s"};
    engine.process(r3);
    engine.process(r4);
    EXPECT_TRUE(triggered);
}
