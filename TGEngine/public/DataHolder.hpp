#pragma once

#include <algorithm>
#include <mutex>
#include <tuple>
#include <unordered_map>
#include <vector>

template <class internaltype>
inline void addInternal(std::vector<internaltype> &allocation,
                        const size_t internalID, const size_t count,
                        const internaltype *values) {
  allocation.resize(allocation.size() + count);
  std::copy(values, values + count, allocation.begin() + internalID);
}

template <class type>
struct DataHolderBase {
  std::vector<type> allocation1;
  std::unordered_map<size_t, size_t> idMap;
  std::mutex protectionLock;
  size_t maxValue = 0;

  template <class internaltype>
  inline std::vector<internaltype> get(std::vector<internaltype> &allocation,
                                       const size_t count, const size_t *ids) {
    std::lock_guard guard(protectionLock);
    std::vector<internaltype> vector;
    vector.resize(count);
    if (idMap.empty()) {
      std::transform(ids, ids + count, vector.begin(),
                     [&](auto input) { return allocation[input]; });
    } else {
      std::transform(ids, ids + count, vector.begin(),
                     [&](auto input) { return allocation[idMap[input]]; });
    }
    return vector;
  }

  template <class internaltype>
  inline internaltype get(std::vector<internaltype> &allocation,
                          const size_t input) {
    std::lock_guard guard(protectionLock);
    if (idMap.empty()) {
      return allocation[input];
    } else {
      return allocation[idMap[input]];
    }
  }

  inline std::pair<size_t, size_t> add(std::vector<type> &allocation,
                                       const size_t count, const type *ids) {
    const auto lastSize = allocation.size();
    allocation.resize(lastSize + count);
    std::copy(ids, ids + count, allocation.begin());
    std::vector<size_t> idList(count);
    for (size_t i = 0; i < count; i++) {
      const auto id = i + lastSize;
      allocation[id] = ids[i];
      idList[i] = id;
    }
    size_t returnID = maxValue;
    if (!idMap.empty()) {
      returnID = maxValue;
      for (const auto id : idList) {
        idMap[maxValue++] = id;
      }
    } else {
      maxValue += count;
    }
    return std::make_pair(lastSize, returnID);
  }

  inline size_t add(const size_t count, const type *ids) {
    std::lock_guard guard(protectionLock);
    return std::get<1>(add(allocation1, count, ids));
  }

  template <class T>
  size_t size(T &t, size_t count) {
    t.resize(count);
    return 0;
  }

  template <class... Type>
  size_t unpack(Type &&...types) {
    return sizeof...(types);
  }

  template <class... Types>
  inline std::pair<size_t, size_t> allocate(const size_t count,
                                            Types &...types) {
    const auto currentSize = allocation1.size();
    const auto allocationSize = currentSize + count;
    allocation1.resize(allocationSize);
    unpack((size(types, allocationSize))...);
    const auto oldValue = maxValue;
    maxValue += count;
    if (!idMap.empty()) {
      for (size_t i = 0; i < count; i++) {
        const auto current = oldValue + i;
        idMap[current] = currentSize + current;
      }
    }
    return std::pair(currentSize, oldValue);
  }
};

template <class type1, class type2>
struct DataHolder2 : public DataHolderBase<type1> {
  std::vector<type2> allocation2;

  size_t add(const size_t count, const type1 *first, const type2 *second) {
    std::lock_guard guard(DataHolderBase<type1>::protectionLock);
    const auto [internalID, externalID] = DataHolderBase<type1>::add(
        DataHolderBase<type1>::allocation1, count, first);
    addInternal(allocation2, internalID, count, second);
    return externalID;
  }

  inline std::pair<size_t, size_t> allocate1(const size_t count) {
    return DataHolderBase<type1>::allocate(count, allocation2);
  }
};

template <class type1, class type2, class type3>
struct DataHolder3 : public DataHolder2<type1, type2> {
  std::vector<type3> allocation3;

  size_t add(const size_t count, const type1 *first, const type2 *second,
             const type2 *third) {
    std::lock_guard guard(DataHolderBase<type1>::protectionLock);
    const auto [internalID, externalID] = DataHolderBase<type1>::add(
        DataHolderBase<type1>::allocation1, count, first);
    addInternal(DataHolder2<type1, type2>::allocation2, internalID, count,
                second);
    addInternal(allocation3, internalID, count, third);
    return externalID;
  }

  inline std::pair<size_t, size_t> allocate2(const size_t count) {
    return DataHolderBase<type1>::allocate(
        count, DataHolder2<type1, type2>::allocation2, allocation3);
  }
};

template <class type1, class type2, class type3, class type4>
struct DataHolder4 : public DataHolder3<type1, type2, type3> {
  std::vector<type4> allocation4;

  size_t add(const size_t count, const type1 *first, const type2 *second,
             const type3 *third, const type4 *fourth) {
    std::lock_guard guard(DataHolderBase<type1>::protectionLock);
    const auto [internalID, externalID] = DataHolderBase<type1>::add(
        DataHolderBase<type1>::allocation1, count, first);
    addInternal(DataHolder2<type1, type2>::allocation2, internalID, count,
                second);
    addInternal(DataHolder3<type1, type2, type3>::allocation3, internalID,
                count, third);
    addInternal(DataHolder4<type1, type2, type3, type4>::allocation4,
                internalID, count, fourth);
    return externalID;
  }

  inline std::pair<size_t, size_t> allocate3(const size_t count) {
    return DataHolderBase<type1>::allocate(
        count, DataHolder2<type1, type2>::allocation2,
        DataHolder3<type1, type2, type3>::allocation3, allocation4);
  }
};

template <class type1, class type2, class type3, class type4, class type5>
struct DataHolder5 : public DataHolder4<type1, type2, type3, type4> {
  std::vector<type5> allocation5;

  size_t add(const size_t count, const type1 *first, const type2 *second,
             const type3 *third, const type4 *fourth, const type5 *fith) {
    std::lock_guard guard(DataHolderBase<type1>::protectionLock);
    const auto [internalID, externalID] = DataHolderBase<type1>::add(
        DataHolderBase<type1>::allocation1, count, first);
    addInternal(DataHolder2<type1, type2>::allocation2, internalID, count,
                second);
    addInternal(DataHolder3<type1, type2, type3>::allocation3, internalID,
                count, third);
    addInternal(DataHolder4<type1, type2, type3, type4>::allocation4,
                internalID, count, fourth);
    addInternal(allocation5, internalID, count, fith);
    return externalID;
  }

  inline std::pair<size_t, size_t> allocate4(const size_t count) {
    return DataHolderBase<type1>::allocate(
        count, DataHolder2<type1, type2>::allocation2,
        DataHolder3<type1, type2, type3>::allocation3,
        DataHolder4<type1, type2, type3, type4>::allocation4, allocation5);
  }

  inline auto start(const size_t count) {
    const auto [internalID, returnID] = allocate4(count);
    const auto first =
        std::begin(DataHolderBase<type1>::allocation1) + internalID;
    const auto second =
        std::begin(DataHolder2<type1, type2>::allocation2) + internalID;
    const auto third =
        std::begin(DataHolder3<type1, type2, type3>::allocation3) + internalID;
    const auto fourth =
        std::begin(DataHolder4<type1, type2, type3, type4>::allocation4) +
        internalID;
    const auto fith = std::begin(allocation5) + internalID;
    return std::tuple(returnID, first, second, third, fourth, fith);
  }
};