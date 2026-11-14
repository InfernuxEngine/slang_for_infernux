#ifndef SLANG_BUILTIN_SPIRV_TOOLS_H
#define SLANG_BUILTIN_SPIRV_TOOLS_H

#include "slang.h"

namespace Slang
{
// Return the compiler-owned SPIR-V backend without consulting library paths.
ISlangSharedLibrary* getBuiltinSpirvToolsLibrary();
} // namespace Slang

#endif
