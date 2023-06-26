#include <gtest/gtest.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>

#include "../public/DataHolder.hpp"

TEST(DataHolderTest, BasicDataHolderTests) {
  static plog::ColorConsoleAppender<plog::TxtFormatter> color;
  plog::init(plog::verbose, &color);

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
            *third = i * i;
            first++;
            second++;
            third++;
          }
        },
        value.iterator);
  }

  for (size_t i = 0; i < AMOUNT; i++) {
    EXPECT_EQ(holder.get<0>(i), i);
    EXPECT_EQ(holder.get<1>(i), -i);
    EXPECT_EQ(holder.get<2>(i), i * i);
  }

  std::vector<size_t> toErase;
  static constexpr auto DELETE = AMOUNT / 2;
  for (size_t i = 0; i < DELETE; i++) {
    toErase.push_back(i * 2);
  }
  EXPECT_TRUE(holder.erase(toErase));

  for (size_t i = 0; i < DELETE; i++) {
    size_t index = 2 * i + 1;
    EXPECT_EQ(holder.get<0>(index), index);
    EXPECT_EQ(holder.get<1>(index), -index);
    EXPECT_EQ(holder.get<2>(index), index * index);
  }

  EXPECT_EQ(std::get<0>(holder.internalValues).size(), AMOUNT);
  EXPECT_EQ(std::get<1>(holder.internalValues).size(), AMOUNT);
  EXPECT_EQ(std::get<2>(holder.internalValues).size(), AMOUNT);

  holder.compact();

  for (size_t i = 0; i < DELETE; i++) {
    size_t index = 2 * i + 1;
    EXPECT_EQ(holder.get<0>(index), index);
    EXPECT_EQ(holder.get<1>(index), -index);
    EXPECT_EQ(holder.get<2>(index), index * index);
  }

  EXPECT_EQ(std::get<0>(holder.internalValues).size(), DELETE);
  EXPECT_EQ(std::get<1>(holder.internalValues).size(), DELETE);
  EXPECT_EQ(std::get<2>(holder.internalValues).size(), DELETE);
}