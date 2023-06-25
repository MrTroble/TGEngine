#include <gtest/gtest.h>

#include "../public/DataHolder.hpp"

TEST(DataHolderTest, BasicDataHolderTests) {
  DataHolder<int, int, int> holder;
  static constexpr size_t AMOUNT = 10;

  {
    auto value = holder.allocate(AMOUNT);
    ASSERT_EQ(value.beginIndex, 0);

    std::apply(
        [&](auto& first, auto& second, auto& third) {
          for (size_t i = 0; i < AMOUNT; i++) {
            *first = i;
            *second = -i;
            *third = i*i;
            first++;
            second++;
            third++;
          }
        },
        value.iterator);
  }
  
  for (size_t i = 0; i < AMOUNT; i++) {
    EXPECT_EQ(holder.get<0>(i), i);
  }

  for (size_t i = 0; i < AMOUNT; i++) {
    EXPECT_EQ(holder.get<1>(i), -i);
  }

  for (size_t i = 0; i < AMOUNT; i++) {
    EXPECT_EQ(holder.get<2>(i), i*i);
  }
}