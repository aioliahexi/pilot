#include <gtest/gtest.h>
#include "engine/ring_buffer.h"

using namespace pilot::engine;

TEST(RingBufferTest, PushPopBasic) {
    RingBuffer<int, 8> rb;
    EXPECT_TRUE(rb.push(42));
    auto val = rb.pop();
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 42);
}

TEST(RingBufferTest, EmptyReturnsNullopt) {
    RingBuffer<int, 8> rb;
    EXPECT_FALSE(rb.pop().has_value());
}

TEST(RingBufferTest, FullReturnsFalse) {
    RingBuffer<int, 8> rb;
    // Capacity is 7 (Capacity - 1 = 8 - 1)
    for (int i = 0; i < 7; ++i) {
        EXPECT_TRUE(rb.push(i)) << "push " << i << " should succeed";
    }
    EXPECT_FALSE(rb.push(99)) << "push to full buffer should fail";
}

TEST(RingBufferTest, FifoOrder) {
    RingBuffer<int, 16> rb;
    for (int i = 0; i < 10; ++i) rb.push(i);
    for (int i = 0; i < 10; ++i) {
        auto v = rb.pop();
        ASSERT_TRUE(v.has_value());
        EXPECT_EQ(*v, i);
    }
}

TEST(RingBufferTest, SizeTracking) {
    RingBuffer<int, 16> rb;
    EXPECT_EQ(rb.size(), 0u);
    rb.push(1);
    EXPECT_EQ(rb.size(), 1u);
    rb.push(2);
    EXPECT_EQ(rb.size(), 2u);
    rb.pop();
    EXPECT_EQ(rb.size(), 1u);
    rb.pop();
    EXPECT_EQ(rb.size(), 0u);
}

TEST(RingBufferTest, EmptyAndFullPredicates) {
    RingBuffer<int, 4> rb;
    EXPECT_TRUE(rb.empty());
    rb.push(1);
    EXPECT_FALSE(rb.empty());
}

TEST(RingBufferTest, WrapAround) {
    RingBuffer<int, 8> rb;
    // Fill 3, drain 3 – advances internal head/tail pointers
    for (int i = 0; i < 3; ++i) rb.push(i);
    for (int i = 0; i < 3; ++i) rb.pop();

    // Now fill to capacity (7 items) across the wrap boundary
    for (int i = 10; i < 17; ++i) {
        EXPECT_TRUE(rb.push(i)) << "push " << i << " should succeed";
    }
    // Buffer is now full; one more push should fail
    EXPECT_FALSE(rb.push(99));

    // Verify FIFO order across the wrap boundary
    for (int i = 10; i < 17; ++i) {
        auto v = rb.pop();
        ASSERT_TRUE(v.has_value());
        EXPECT_EQ(*v, i);
    }
}

TEST(RingBufferTest, CapacityConstant) {
    constexpr std::size_t cap8  = RingBuffer<int, 8>::capacity();
    constexpr std::size_t cap16 = RingBuffer<int, 16>::capacity();
    EXPECT_EQ(cap8,  7u);
    EXPECT_EQ(cap16, 15u);
}
