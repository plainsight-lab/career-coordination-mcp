#pragma once

#include <atomic>
#include <string>
#include <string_view>

namespace ccmcp::core {

// Abstract ID generator interface for dependency injection.
// Allows production code to use timestamp-based IDs while tests/demos use deterministic IDs.
// Following C++ Core Guidelines I.25: Prefer abstract classes as interfaces to class hierarchies.
class IIdGenerator {
 public:
  virtual ~IIdGenerator() = default;

  // Generate next ID with given prefix.
  // Contract: returned ID is non-empty and starts with prefix.
  virtual std::string next(std::string_view prefix) = 0;

 protected:
  IIdGenerator() = default;
  IIdGenerator(const IIdGenerator&) = default;
  IIdGenerator& operator=(const IIdGenerator&) = default;
  IIdGenerator(IIdGenerator&&) = default;
  IIdGenerator& operator=(IIdGenerator&&) = default;
};

// Production ID generator: timestamp (microseconds) + atomic counter for uniqueness.
// Thread-safe. IDs are globally unique within process lifetime and sortable by creation time.
class SystemIdGenerator final : public IIdGenerator {
 public:
  SystemIdGenerator() = default;
  ~SystemIdGenerator() override = default;

  // Not copyable or movable (contains atomic counter)
  SystemIdGenerator(const SystemIdGenerator&) = delete;
  SystemIdGenerator& operator=(const SystemIdGenerator&) = delete;
  SystemIdGenerator(SystemIdGenerator&&) = delete;
  SystemIdGenerator& operator=(SystemIdGenerator&&) = delete;

  std::string next(std::string_view prefix) override;

 private:
  std::atomic<unsigned long long> counter_{0};
};

// Deterministic ID generator: sequential counter only, no timestamps.
// For tests and demos where reproducible output is required.
// Thread-safe. IDs are deterministic: same sequence of next() calls produces same IDs.
class DeterministicIdGenerator final : public IIdGenerator {
 public:
  DeterministicIdGenerator() = default;
  ~DeterministicIdGenerator() override = default;

  // Not copyable or movable (contains atomic counter)
  DeterministicIdGenerator(const DeterministicIdGenerator&) = delete;
  DeterministicIdGenerator& operator=(const DeterministicIdGenerator&) = delete;
  DeterministicIdGenerator(DeterministicIdGenerator&&) = delete;
  DeterministicIdGenerator& operator=(DeterministicIdGenerator&&) = delete;

  std::string next(std::string_view prefix) override;

 private:
  std::atomic<unsigned long long> counter_{0};
};

}  // namespace ccmcp::core
