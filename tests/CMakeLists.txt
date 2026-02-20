add_executable(ccmcp_tests
  test_ids.cpp
  test_validation_engine.cpp
  test_matching_stub.cpp
  test_domain_schema.cpp
  test_matcher_basic_overlap.cpp
  test_matcher_determinism.cpp
  test_matcher_tie_break.cpp
  test_matcher_missing.cpp
  test_matcher_hybrid_retrieval.cpp
  test_matcher_hybrid_determinism.cpp
  test_validation_schema_block.cpp
  test_validation_evidence_fail.cpp
  test_validation_warn.cpp
  test_validation_pass.cpp
  test_inmemory_atom_repository.cpp
  test_inmemory_opportunity_repository.cpp
  test_inmemory_interaction_repository.cpp
  test_inmemory_interaction_coordinator.cpp
  test_redis_interaction_coordinator.cpp
  test_null_embedding_index.cpp
  test_inmemory_embedding_index.cpp
  test_sqlite_embedding_index.cpp
  test_sqlite_atom_repository.cpp
  test_sqlite_opportunity_repository.cpp
  test_sqlite_audit_log.cpp
  test_sqlite_resume_store.cpp
  test_sqlite_resume_token_store.cpp
  test_tokenization.cpp
  test_token_ir_validation.cpp
  test_format_adapter.cpp
  test_hygiene.cpp
  test_resume_ingestor.cpp
  test_app_match_pipeline.cpp
  test_app_interaction_pipeline.cpp
  test_sqlite_index_run_store.cpp
  test_index_build_pipeline.cpp
  test_app_service_ingest_pipeline.cpp
  test_app_service_index_build_pipeline.cpp
  test_decision_record.cpp
  test_sqlite_decision_store.cpp
  test_vector_backend.cpp
  test_override_rail.cpp
  test_redis_config.cpp
  test_startup_guard.cpp
  test_runtime_config_snapshot.cpp
  test_interaction_ordering.cpp
  ../apps/mcp_server/startup_guard.cpp
)

target_link_libraries(ccmcp_tests
  PRIVATE
    ccmcp
    Catch2::Catch2WithMain
)

# Allow test_startup_guard.cpp to include startup_guard.h and config.h
# (private mcp_server headers that are not part of the ccmcp library).
target_include_directories(ccmcp_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../apps/mcp_server
)

include(Catch)
catch_discover_tests(ccmcp_tests)
