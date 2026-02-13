#pragma once

#include <utility>
#include <variant>

namespace ccmcp::core {

// Error enumerations following E.14 (use purpose-designed types as error indicators).
// These domain-specific errors carry semantic meaning beyond built-in error codes.

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

// Result<T, E> follows C++ Core Guidelines E.27: systematic error handling without exceptions.
// This type encodes success (T) or failure (E) explicitly, preventing ignored errors.
// Usage: return Result<Value, ErrorType>::ok(val) or Result<Value, ErrorType>::err(error).
// The has_value() check makes error handling mandatory and visible at call sites.
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
