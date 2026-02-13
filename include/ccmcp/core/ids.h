#pragma once

#include <atomic>
#include <chrono>
#include <string>

namespace ccmcp::core {

struct AtomId {
  std::string value;
  bool operator==(const AtomId&) const = default;
};

struct OpportunityId {
  std::string value;
  bool operator==(const OpportunityId&) const = default;
};

struct ContactId {
  std::string value;
  bool operator==(const ContactId&) const = default;
};

struct InteractionId {
  std::string value;
  bool operator==(const InteractionId&) const = default;
};

struct ResumeId {
  std::string value;
  bool operator==(const ResumeId&) const = default;
};

struct TraceId {
  std::string value;
  bool operator==(const TraceId&) const = default;
};

inline std::string make_id(const char* prefix) {
  static std::atomic<unsigned long long> counter{0};
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  const auto c = counter.fetch_add(1, std::memory_order_relaxed);
  return std::string(prefix) + "-" + std::to_string(micros) + "-" + std::to_string(c);
}

inline AtomId new_atom_id() { return AtomId{make_id("atom")}; }
inline OpportunityId new_opportunity_id() { return OpportunityId{make_id("opp")}; }
inline ContactId new_contact_id() { return ContactId{make_id("contact")}; }
inline InteractionId new_interaction_id() { return InteractionId{make_id("interaction")}; }
inline ResumeId new_resume_id() { return ResumeId{make_id("resume")}; }
inline TraceId new_trace_id() { return TraceId{make_id("trace")}; }

}  // namespace ccmcp::core
