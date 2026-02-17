#include "ccmcp/constitution/token_ir_artifact_view.h"

namespace ccmcp::constitution {

TokenIRArtifactView::TokenIRArtifactView(const domain::ResumeTokenIR& token_ir,
                                         const std::string& canonical_resume_hash,
                                         const std::string& canonical_resume_text)
    : token_ir_(token_ir),
      canonical_resume_hash_(canonical_resume_hash),
      canonical_resume_text_(canonical_resume_text) {}

}  // namespace ccmcp::constitution
