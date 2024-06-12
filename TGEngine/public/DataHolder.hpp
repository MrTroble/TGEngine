#pragma once

#include <plog/Log.h>

#include <algorithm>
#include <mutex>
#include <span>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tge {

template <class Type>
concept HolderConcept = requires(Type t) { t.internalHandle; };

template <class internaltype>
inline void addInternal(std::vector<internaltype> &allocation,
                        const size_t internalID, const size_t count,
                        const internaltype *values) {
  allocation.resize(allocation.size() + count);
  std::copy(values, values + count, allocation.begin() + internalID);
}

template <class... ExternalTypes>
struct DataHolderOutput : public std::unique_lock<std::mutex> {
  std::tuple<typename std::vector<ExternalTypes>::iterator...> iterator;
  size_t beginIndex;

  explicit DataHolderOutput(std::mutex &mutex)
      : std::unique_lock<std::mutex>(mutex), beginIndex(0) {}

  template <HolderConcept Holder>
  inline std::vector<Holder> generateOutputArray(const size_t count) {
    std::vector<Holder> outputHolder(count);
    for (size_t i = 0; i < count; i++) {
      outputHolder[i] = Holder(beginIndex + i);
    }
    return outputHolder;
  }
};

template <typename Tuple, typename Tuple2, std::size_t... I>
void process(Tuple &lists, Tuple2 &values, std::index_sequence<I...> _i) {
  (std::get<I>(lists).push_back(std::get<I>(values)), ...);
}

template <typename Tuple, typename Tuple2>
void process(Tuple &tuple, Tuple2 &values) {
  process(tuple, values,
          std::make_index_sequence<std::tuple_size<Tuple>::value>());
}

template <class Type>
struct DataHolderSingleOutput : public std::unique_lock<std::mutex> {
  Type &data;

  explicit DataHolderSingleOutput(std::mutex &mutex, Type &data)
      : std::unique_lock<std::mutex>(mutex), data(data) {}

  explicit DataHolderSingleOutput(std::unique_lock<std::mutex> &&mutex,
                                  Type &data)
      : std::unique_lock<std::mutex>(std::move(mutex)), data(data) {}

  DataHolderSingleOutput<Type> &operator=(const Type &other) {
    data = other;
    return *this;
  }
};

template <class... ExternalTypes>
struct DataHolder {
  using Outputname = DataHolderOutput<ExternalTypes...>;
  using ValueType = std::tuple<std::vector<ExternalTypes>...>;

  template <size_t index>
  using TypeAt = std::tuple_element_t<index, std::tuple<ExternalTypes...>>;

  std::mutex mutex;
  ValueType internalValues;
  std::unordered_map<size_t, size_t> translationTable;
  size_t currentIndex = 0;

 protected:
  template <size_t index = 0>
  size_t __size() {
    auto &vector = std::get<index>(internalValues);
    return vector.size();
  }

 public:
  template <size_t index = 0, HolderConcept HolderType>
  std::vector<TypeAt<index>> get(const std::span<HolderType> &pIndex) {
    std::vector<size_t> indicies(pIndex.size());
    auto start = indicies.begin();
    for (auto holder : pIndex) {
      *(start++) = holder.internalHandle;
    }
    return get<index>(indicies);
  }

  template <size_t index = 0, HolderConcept HolderType>
  TypeAt<index> get(HolderType pIndex) {
    return get<index>(pIndex.internalHandle);
  }

  template <size_t index = 0>
  std::vector<TypeAt<index>> get(const std::span<size_t> &pIndex) {
    std::lock_guard guard(this->mutex);
    std::vector<TypeAt<index>> types;
    types.reserve(pIndex.size());
    auto &vector = std::get<index>(internalValues);
    for (const auto index : pIndex) {
      auto newIndex = translationTable.find(index);
      if (newIndex == std::end(translationTable)) {
        PLOG_ERROR << "Index " << index << " not in DataHolder!";
        throw std::runtime_error("Index not in DataHolder!");
      }
      types.push_back(vector[newIndex->second]);
    }
    return types;
  }

  template <size_t index = 0>
  size_t size() {
    std::lock_guard guard(mutex);
    return __size<index>();
  }

  template <size_t index = 0>
  TypeAt<index> get(const size_t pIndex) {
    std::lock_guard guard(mutex);
    auto newIndex = translationTable.find(pIndex);
    if (newIndex == std::end(translationTable)) {
      PLOG_ERROR << "Index " << pIndex << " not in DataHolder!";
      throw std::runtime_error("Index not in DataHolder!");
    }
    auto &vector = std::get<index>(internalValues);
    return vector[newIndex->second];
  }

  template <size_t index = 0>
  DataHolderSingleOutput<TypeAt<index>> change(const size_t pIndex) {
    std::unique_lock guard(this->mutex);
    auto &vector = std::get<index>(internalValues);
    auto newIndex = translationTable.find(pIndex);
#ifdef DEBUG
    if (newIndex == std::end(translationTable)) {
      PLOG_ERROR << "Index " << pIndex << " not in DataHolder!";
      throw std::runtime_error("Index not in DataHolder!");
    }
#endif // DEBUG
    return DataHolderSingleOutput<TypeAt<index>>(std::move(guard),
                                                 vector[newIndex->second]);
  }


  template <size_t index = 0, HolderConcept HolderType>
  void changeAll(const std::span<const HolderType> pIndex, std::invocable<TypeAt<index>> auto invocable) {
      std::unique_lock guard(this->mutex);
      auto& vector = std::get<index>(internalValues);
      for (const auto holder : pIndex)
      {
          auto newIndex = translationTable.find(holder.internalHandle);
#ifdef DEBUG
          if (newIndex == std::end(translationTable)) {
              PLOG_ERROR << "Index " << holder.internalHandle << " not in DataHolder!";
              throw std::runtime_error("Index not in DataHolder!");
          }
#endif // DEBUG
          vector[newIndex->second] = invocable(vector[newIndex->second]);
      }
  }

  template <size_t index = 0, HolderConcept HolderType>
  DataHolderSingleOutput<TypeAt<index>> change(const HolderType pIndex) {
    return change<index>(pIndex.internalHandle);
  }

  Outputname allocate(const size_t amount) {
    Outputname holder(this->mutex);
    const auto index = std::get<0>(internalValues).size();
    holder.beginIndex = currentIndex;
    const auto newSize = index + amount;
    holder.iterator = std::apply(
        [&](auto &...input) {
          (input.resize(newSize), ...);
          return std::make_tuple((std::begin(input) + index)...);
        },
        internalValues);
    for (size_t i = 0; i < amount; i++) {
      translationTable[currentIndex++] = index + i;
    }
    return holder;
  }

  bool erase(const std::span<size_t> toErase) {
    const std::lock_guard guard(mutex);
    for (const size_t key : toErase) {
      if (translationTable.erase(key) == 0) return false;
    }
    return true;
  }

  template <HolderConcept Holder>
  bool erase(const std::span<Holder> toErase) {
    const std::lock_guard guard(mutex);
    for (const auto key : toErase) {
      if (translationTable.erase(key.internalHandle) == 0) return false;
    }
    return true;
  }

  ValueType compact() {
    std::lock_guard guard(mutex);
    auto index = translationTable.size();
    ValueType newValue;
    std::apply([&](auto &...vectors) { (vectors.reserve(index), ...); },
               newValue);
    size_t currentIndex = 0;
    std::unordered_set<size_t> oldValues;
    const auto oldSize = __size();
    oldValues.reserve(oldSize - index);
    for (auto &[key, value] : translationTable) {
      oldValues.insert(value);
      const auto values = std::apply(
          [&](const auto &...old) { return std::make_tuple(old[value]...); },
          internalValues);
      process(newValue, values);
      value = currentIndex;
      currentIndex++;
    }
    ValueType oldValueType;
    std::apply([&](auto &...vectors) { (vectors.reserve(oldSize), ...); },
               oldValueType);
    for (size_t i = 0; i < oldSize; i++) {
      if (oldValues.contains(i)) continue;
      const auto values =
          std::apply([&](auto &...old) { return std::make_tuple(old[i]...); },
                     internalValues);
      process(oldValueType, values);
    }
    internalValues = newValue;
    return oldValueType;
  }

  template <size_t index = 0>
  void fill_adjacent(size_t startIndex, const TypeAt<index> &value,
                     const size_t end = SIZE_MAX) {
    const std::lock_guard guard(this->mutex);
    auto &values = std::get<index>(internalValues);
    const auto insert = translationTable.at(startIndex);
    const auto realEnd = std::min(end, values.size() - insert) + insert;
    for (size_t i = insert; i < realEnd; i++) {
      values[i] = value;
    }
  }

  ValueType clear() {
    translationTable.clear();
    ValueType currentValues = std::move(this->internalValues);
    this->internalValues = ValueType();
    return currentValues;
  }
};

}  // namespace tge
