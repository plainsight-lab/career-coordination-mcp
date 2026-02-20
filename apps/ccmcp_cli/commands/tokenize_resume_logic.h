#pragma once

#include "ccmcp/ingest/resume_store.h"
#include "ccmcp/tokenization/resume_token_store.h"

#include <string>

// execute_tokenize_resume: look up a resume by ID, tokenize it, persist the token IR, and print
// results. Takes only interface types — no concrete storage headers may be included in this TU.
// The tokenizer itself is constructed inside the function based on mode.
int execute_tokenize_resume(const std::string& resume_id_str, const std::string& mode,
                            ccmcp::ingest::IResumeStore& resume_store,
                            ccmcp::tokenization::IResumeTokenStore& token_store);
