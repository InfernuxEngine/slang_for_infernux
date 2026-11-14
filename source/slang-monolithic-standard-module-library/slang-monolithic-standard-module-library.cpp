#include "core/slang-lz4-compression-system.h"
#include "slang/slang-standard-modules.h"

#include <cstring>
#include <mutex>

namespace Slang
{
namespace
{
struct EmbeddedStandardModule
{
    const char* path;
    const unsigned char* data;
    size_t compressedSize;
    size_t uncompressedSize;
};

#include "slang-monolithic-standard-module-library-generated.cpp"

struct ModuleStorage
{
    std::once_flag initialized;
    SlangResult status = SLANG_FAIL;
    ComPtr<ISlangBlob> blob;
};
ModuleStorage g_storage[g_standardModuleCount];
} // namespace

SlangResult loadEmbeddedStandardModule(const String& moduleName, ISlangBlob** outBlob)
{
    *outBlob = nullptr;
    for (size_t i = 0; i < g_standardModuleCount; ++i)
    {
        const auto& module = g_standardModules[i];
        if (std::strcmp(module.path, moduleName.getBuffer()) != 0)
            continue;

        auto& storage = g_storage[i];
        std::call_once(
            storage.initialized,
            [&]()
            {
                List<uint8_t> bytes;
                bytes.setCount(Index(module.uncompressedSize));
                storage.status = LZ4CompressionSystem::getSingleton()->decompress(
                    module.data,
                    module.compressedSize,
                    module.uncompressedSize,
                    bytes.getBuffer());
                if (SLANG_SUCCEEDED(storage.status))
                    storage.blob = ListBlob::moveCreate(bytes);
            });
        if (storage.blob)
        {
            *outBlob = storage.blob.get();
            (*outBlob)->addRef();
        }
        return storage.status;
    }
    return SLANG_E_NOT_FOUND;
}
} // namespace Slang
