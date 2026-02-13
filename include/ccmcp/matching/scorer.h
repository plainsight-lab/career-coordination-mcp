#pragma once

namespace ccmcp::matching {

struct ScoreWeights {
  double lexical{0.55};
  double semantic{0.35};
  double bonus{0.10};
};

}  // namespace ccmcp::matching
