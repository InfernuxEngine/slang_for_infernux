# The built versioned module tree. $<CONFIG> is resolved when the custom command
# runs. This uses the build-tree subdir the producers actually write to -- not
# SLANG_STANDARD_MODULE_INSTALL_DIR, which is the (overridable) install
# destination and may point elsewhere.
set(embed_module_root
    "${slang_BINARY_DIR}/$<CONFIG>/${SLANG_STANDARD_MODULE_BUILD_SUBDIR}"
)

# Generate the payload source per-configuration so multi-config builds (e.g.
# Debug and Release, whose module roots differ by $<CONFIG>) do not race on, or
# reuse, one another's generated source. The include directory carries $<CONFIG>
# to match, so the wrapper's #include resolves to this configuration's payload.
set(embed_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>")
set(embed_generated_source
    "${embed_generated_dir}/slang-monolithic-standard-module-library-generated.cpp"
)

# Scan the built tree, LZ4-compress each module, and emit the embedded byte
# arrays + lookup table. The generator links core for LZ4CompressionSystem; the
# compiler decompresses with the same system at load.
#
# DEPENDS lists both the producer targets and their output files:
#   - the module targets enforce build ordering (under Visual Studio, file-level
#     dependencies across directories do not order the generating projects, so
#     without these the embedding can start before every .slang-module exists);
#   - the SLANG_STANDARD_MODULE_FILES manifest tracks the actual outputs, so
#     editing a module regenerates the payload instead of leaving it stale.
add_custom_command(
    OUTPUT ${embed_generated_source}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${embed_generated_dir}
    COMMAND
        slang-generate-monolithic-standard-module-library ${embed_module_root}
        ${embed_generated_source}
    DEPENDS
        slang-generate-monolithic-standard-module-library
        ${SLANG_STANDARD_MODULE_TARGETS}
        ${SLANG_STANDARD_MODULE_FILES}
    COMMENT "Embedding standard modules (LZ4) into ${embed_generated_source}"
    VERBATIM
)
add_custom_target(
    generate_standard_modules_embed_source
    DEPENDS ${embed_generated_source}
)
set_target_properties(
    generate_standard_modules_embed_source
    PROPERTIES FOLDER generated
)

# Link the payload into the actual compiler. No runtime loader, sidecar library,
# or static-registration side effect is required, including for Wasm archives.
target_sources(
    slang
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/slang-monolithic-standard-module-library.cpp"
)
target_include_directories(slang PRIVATE "${embed_generated_dir}")
add_dependencies(slang generate_standard_modules_embed_source)
