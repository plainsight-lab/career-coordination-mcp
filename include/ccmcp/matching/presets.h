#pragma once

#include "ccmcp/matching/scorer.h"

namespace ccmcp::matching {

inline ScoreWeights job_matching_preset() {
  return ScoreWeights{0.55, 0.35, 0.10};
}

inline ScoreWeights corpus_preset() {
  return ScoreWeights{0.35, 0.55, 0.10};
}

}  // namespace ccmcp::matching
