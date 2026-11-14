#ifndef SLANG_STANDARD_MODULES_H
#define SLANG_STANDARD_MODULES_H

#include "core/slang-blob.h"

namespace Slang
{
// Bytes belong to the compiler build. Successful imports share one immutable
// decompressed blob across sessions; unknown module names return NOT_FOUND.
SlangResult loadEmbeddedStandardModule(const String& moduleName, ISlangBlob** outBlob);
} // namespace Slang

#endif
