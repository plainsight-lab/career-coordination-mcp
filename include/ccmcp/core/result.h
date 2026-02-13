#pragma once

#include <utility>
#include <variant>

namespace ccmcp::core {

enum class GovernanceError {
  kInvalidTransition,
  kPolicyViolation,
  kOverrideRequired,
};

enum class StorageError {
  kNotFound,
  kConflict,
  kUnavailable,
};

enum class ParseError {
  kInvalidFormat,
  kMissingField,
  kUnsupportedVersion,
};

template <typename T, typename E>
class Result {
 public:
  static Result ok(T value) { return Result(std::move(value)); }
  static Result err(E error) { return Result(std::move(error)); }

  [[nodiscard]] bool has_value() const { return std::holds_alternative<T>(data_); }
  [[nodiscard]] const T& value() const { return std::get<T>(data_); }
  [[nodiscard]] const E& error() const { return std::get<E>(data_); }

 private:
  explicit Result(T value) : data_(std::move(value)) {}
  explicit Result(E error) : data_(std::move(error)) {}

  std::variant<T, E> data_;
};

}  // namespace ccmcp::core
