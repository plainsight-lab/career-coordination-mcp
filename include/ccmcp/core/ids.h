#pragma once

#include <atomic>
#include <chrono>
#include <string>

namespace ccmcp::core {

struct AtomId {
  std::string value;
};

struct OpportunityId {
  std::string value;
};

struct ContactId {
  std::string value;
};

struct InteractionId {
  std::string value;
};

struct ResumeId {
  std::string value;
};

struct TraceId {
  std::string value;
};

inline bool operator==(const AtomId&, const AtomId&) = default;
inline bool operator==(const OpportunityId&, const OpportunityId&) = default;
inline bool operator==(const ContactId&, const ContactId&) = default;
inline bool operator==(const InteractionId&, const InteractionId&) = default;
inline bool operator==(const ResumeId&, const ResumeId&) = default;
inline bool operator==(const TraceId&, const TraceId&) = default;

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
