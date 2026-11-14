# Embedded standard modules

`SLANG_EMBED_STANDARD_MODULES=ON` embeds every serialized standard module directly
in the compiler. The normal compiler contains the compressed bytes; its bootstrap
compiler contains only the lookup stub and generates the payload without a cycle.
No additional runtime library or loose module directory is installed.

The existing host generator scans the built module manifest, compresses with LZ4,
and emits the lookup table per build configuration. Both producer targets and
serialized outputs are dependencies, so source changes rebuild the payload.
Cross builds run the same host generator rather than executing target binaries.

Imports deserialize the same module IR and retain the same experimental-feature
rules. Each module is decompressed once, on its first import; the immutable blob
is shared across compiler sessions. Modules not imported use no decompression
memory. Neural, differentiable, numeric, and functional modules remain included.
