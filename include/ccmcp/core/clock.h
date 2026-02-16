#pragma once

#include <string>

namespace ccmcp::core {

// Abstract clock interface for timestamp injection.
// Allows production code to use system time while tests/demos use fixed timestamps.
// Following C++ Core Guidelines I.25: Prefer abstract classes as interfaces to class hierarchies.
class IClock {
 public:
  virtual ~IClock() = default;

  // Return current timestamp in ISO 8601 format (UTC).
  // Contract: returned string is non-empty and valid ISO 8601.
  virtual std::string now_iso8601() = 0;

 protected:
  IClock() = default;
  IClock(const IClock&) = default;
  IClock& operator=(const IClock&) = default;
  IClock(IClock&&) = default;
  IClock& operator=(IClock&&) = default;
};

// Production clock: returns actual system time.
class SystemClock final : public IClock {
 public:
  SystemClock() = default;
  ~SystemClock() override = default;

  SystemClock(const SystemClock&) = default;
  SystemClock& operator=(const SystemClock&) = default;
  SystemClock(SystemClock&&) = default;
  SystemClock& operator=(SystemClock&&) = default;

  std::string now_iso8601() override;
};

// Fixed clock: returns constant timestamp for deterministic tests/demos.
class FixedClock final : public IClock {
 public:
  explicit FixedClock(std::string fixed_time) : fixed_time_(std::move(fixed_time)) {}
  ~FixedClock() override = default;

  FixedClock(const FixedClock&) = default;
  FixedClock& operator=(const FixedClock&) = default;
  FixedClock(FixedClock&&) = default;
  FixedClock& operator=(FixedClock&&) = default;

  std::string now_iso8601() override;

 private:
  std::string fixed_time_;
};

}  // namespace ccmcp::core
