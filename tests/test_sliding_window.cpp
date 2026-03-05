#include <gtest/gtest.h>
#include "engine/sliding_window.h"
#include "sensors/sensor_base.h"
#include <chrono>

using namespace pilot::engine;
using pilot::sensors::SensorReading;

static SensorReading make_reading(double value,
                                  std::chrono::system_clock::time_point tp,
                                  const std::string& id = "s0") {
    return SensorReading{tp, value, "unit", id};
}

TEST(SlidingWindowTest, EmptyReturnsNullopt) {
    SlidingWindow<> w(std::chrono::seconds(10));
    EXPECT_FALSE(w.stats().has_value());
    EXPECT_TRUE(w.empty());
    EXPECT_EQ(w.size(), 0u);
}

TEST(SlidingWindowTest, SingleElement) {
    SlidingWindow<> w(std::chrono::seconds(10));
    auto now = std::chrono::system_clock::now();
    w.push(make_reading(5.0, now));
    auto s = w.stats();
    ASSERT_TRUE(s.has_value());
    EXPECT_DOUBLE_EQ(s->min_val, 5.0);
    EXPECT_DOUBLE_EQ(s->max_val, 5.0);
    EXPECT_DOUBLE_EQ(s->avg_val, 5.0);
    EXPECT_EQ(s->count, 1u);
}

TEST(SlidingWindowTest, MinMaxAvg) {
    SlidingWindow<> w(std::chrono::seconds(60));
    auto now = std::chrono::system_clock::now();
    w.push(make_reading(10.0, now));
    w.push(make_reading(20.0, now));
    w.push(make_reading(30.0, now));
    auto s = w.stats();
    ASSERT_TRUE(s.has_value());
    EXPECT_DOUBLE_EQ(s->min_val, 10.0);
    EXPECT_DOUBLE_EQ(s->max_val, 30.0);
    EXPECT_DOUBLE_EQ(s->avg_val, 20.0);
    EXPECT_EQ(s->count, 3u);
}

TEST(SlidingWindowTest, EvictsOldReadings) {
    SlidingWindow<> w(std::chrono::seconds(5));
    auto past = std::chrono::system_clock::now() - std::chrono::seconds(10);
    auto now  = std::chrono::system_clock::now();

    // Push an old reading that should be evicted
    w.push(make_reading(999.0, past));
    // Push a recent reading
    w.push(make_reading(42.0, now));

    // Eviction happens on next push or explicit evict_old
    w.evict_old();

    auto s = w.stats();
    ASSERT_TRUE(s.has_value());
    // Old reading should have been evicted
    EXPECT_DOUBLE_EQ(s->avg_val, 42.0);
    EXPECT_EQ(s->count, 1u);
}

TEST(SlidingWindowTest, AllEvicted) {
    SlidingWindow<> w(std::chrono::seconds(1));
    auto past = std::chrono::system_clock::now() - std::chrono::seconds(5);
    w.push(make_reading(1.0, past));
    w.push(make_reading(2.0, past));
    w.evict_old();
    EXPECT_TRUE(w.empty());
    EXPECT_FALSE(w.stats().has_value());
}

TEST(SlidingWindowTest, SizeAfterPush) {
    SlidingWindow<> w(std::chrono::seconds(60));
    auto now = std::chrono::system_clock::now();
    EXPECT_EQ(w.size(), 0u);
    w.push(make_reading(1.0, now));
    EXPECT_EQ(w.size(), 1u);
    w.push(make_reading(2.0, now));
    EXPECT_EQ(w.size(), 2u);
}

TEST(SlidingWindowTest, DataAccessible) {
    SlidingWindow<> w(std::chrono::seconds(60));
    auto now = std::chrono::system_clock::now();
    w.push(make_reading(7.0, now));
    w.push(make_reading(8.0, now));
    ASSERT_EQ(w.data().size(), 2u);
    EXPECT_DOUBLE_EQ(w.data()[0].value, 7.0);
    EXPECT_DOUBLE_EQ(w.data()[1].value, 8.0);
}
