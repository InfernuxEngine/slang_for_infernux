#include "slang/slang-standard-modules.h"

namespace Slang
{
SlangResult loadEmbeddedStandardModule(const String&, ISlangBlob** outBlob)
{
    *outBlob = nullptr;
    return SLANG_E_NOT_FOUND;
}
} // namespace Slang
