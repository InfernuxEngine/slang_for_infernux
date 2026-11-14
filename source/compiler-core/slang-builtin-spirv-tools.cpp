#include "slang-builtin-spirv-tools.h"

#if SLANG_ENABLE_BUILTIN_SPIRV_TOOLS

#include "core/slang-shared-library.h"
#include "core/slang-string.h"
#include "slang-com-helper.h"
#include "slang-glslang/slang-glslang.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <spirv-tools/libspirv.hpp>
#include <spirv-tools/linker.hpp>
#include <spirv-tools/optimizer.hpp>
#include <vector>

namespace Slang
{
namespace
{
spv_target_env getSpirvEnvironment(const uint32_t* words)
{
    static const spv_target_env environments[] = {
        SPV_ENV_UNIVERSAL_1_0,
        SPV_ENV_UNIVERSAL_1_1,
        SPV_ENV_UNIVERSAL_1_2,
        SPV_ENV_UNIVERSAL_1_3,
        SPV_ENV_UNIVERSAL_1_4,
        SPV_ENV_UNIVERSAL_1_5,
        SPV_ENV_UNIVERSAL_1_6,
    };
    return environments[(words[1] >> 8) & 0xff];
}

// Retain Slang's downstream protocol, while providing its SPIR-V operations
// inside the compiler. No GLSL parser or dynamically located module is needed.
int optimizeSpirv(glslang_CompileRequest_1_3* request)
{
    const auto* words = static_cast<const uint32_t*>(request->inputBegin);
    const auto count = (static_cast<const char*>(request->inputEnd) -
                        static_cast<const char*>(request->inputBegin)) /
                       sizeof(uint32_t);
    const auto environment = getSpirvEnvironment(words);
    spvtools::Optimizer optimizer(environment);
    optimizer.SetMessageConsumer(
        [request](
            spv_message_level_t level,
            const char*,
            const spv_position_t& position,
            const char* message)
        {
            static const char* const severities[] =
                {"error", "error", "error", "warning", "note", "note"};
            StringBuilder text;
            text << severities[level] << ":spirv-opt:" << position.line << ": " << message << "\n";
            request->diagnosticFunc(
                text.getBuffer(),
                text.getLength(),
                request->diagnosticUserData);
        });
    switch (request->optimizationLevel)
    {
    case SLANG_OPTIMIZATION_LEVEL_NONE:
        break;
    case SLANG_OPTIMIZATION_LEVEL_DEFAULT:
        optimizer.RegisterPass(spvtools::CreateWrapOpKillPass());
        optimizer.RegisterPass(spvtools::CreateDeadBranchElimPass());
        optimizer.RegisterPass(spvtools::CreateMergeReturnPass());
        optimizer.RegisterPass(spvtools::CreateInlineExhaustivePass());
        optimizer.RegisterPass(spvtools::CreateEliminateDeadFunctionsPass());
        optimizer.RegisterPass(spvtools::CreateAggressiveDCEPass());
        optimizer.RegisterPass(spvtools::CreatePrivateToLocalPass());
        optimizer.RegisterPass(spvtools::CreateScalarReplacementPass(100));
        optimizer.RegisterPass(spvtools::CreateCCPPass());
        optimizer.RegisterPass(spvtools::CreateSimplificationPass());
        optimizer.RegisterPass(spvtools::CreateLocalAccessChainConvertPass());
        optimizer.RegisterPass(spvtools::CreateLocalSingleBlockLoadStoreElimPass());
        optimizer.RegisterPass(spvtools::CreateAggressiveDCEPass());
        optimizer.RegisterPass(spvtools::CreateVectorDCEPass());
        break;
    case SLANG_OPTIMIZATION_LEVEL_HIGH:
    case SLANG_OPTIMIZATION_LEVEL_MAXIMAL:
        optimizer.RegisterPerformancePasses();
        break;
    }
    for (size_t index = 0; index < request->spirvOptimizationFlagCount; ++index)
        if (!optimizer.RegisterPassFromFlag(request->spirvOptimizationFlags[index]))
            return -1;
    spvtools::OptimizerOptions options;
    options.set_max_id_bound(0x3FFFFFFF);
    options.set_run_validator(false);
    std::vector<uint32_t> optimized;
    if (!optimizer.Run(words, count, &optimized, options))
        return -1;
    // Compact only modules whose ID bound would exceed the next tool's default.
    if (optimized[3] > 0x3FFFFF)
    {
        spvtools::Optimizer compact(environment);
        compact.RegisterPass(spvtools::CreateCompactIdsPass());
        std::vector<uint32_t> compacted;
        if (!compact.Run(optimized.data(), optimized.size(), &compacted, options))
            return -1;
        optimized.swap(compacted);
    }
    request->outputFunc(
        optimized.data(),
        optimized.size() * sizeof(uint32_t),
        request->outputUserData);
    return 0;
}

bool validateSpirv(const uint32_t* words, int count)
{
    spvtools::SpirvTools tools(getSpirvEnvironment(words));
    return tools.Validate(words, size_t(count));
}

bool disassembleSpirvWithResult(const uint32_t* words, int count, char** output)
{
    spvtools::SpirvTools tools(getSpirvEnvironment(words));
    std::string text;
    if (!tools.Disassemble(words, size_t(count), &text))
        return false;
    *output = static_cast<char*>(malloc(text.size() + 1));
    if (!*output)
        return false;
    memcpy(*output, text.c_str(), text.size() + 1);
    return true;
}

void freeDisassembly(char* text)
{
    free(text);
}

bool disassembleSpirv(const uint32_t* words, int count)
{
    char* text = nullptr;
    if (!disassembleSpirvWithResult(words, count, &text))
        return false;
    fputs(text, stderr);
    freeDisassembly(text);
    return true;
}

int linkSpirv(glslang_LinkRequest* request)
{
    int newest = 0;
    for (int index = 1; index < request->moduleCount; ++index)
        if (request->modules[index][1] > request->modules[newest][1])
            newest = index;
    spvtools::Context context(getSpirvEnvironment(request->modules[newest]));
    std::vector<uint32_t> linked;
    std::vector<size_t> sizes(request->moduleCount);
    for (int index = 0; index < request->moduleCount; ++index)
        sizes[index] = request->moduleSizes[index];
    spvtools::LinkerOptions options;
    options.SetUseHighestVersion(true);
    if (spvtools::Link(
            context,
            request->modules,
            sizes.data(),
            size_t(request->moduleCount),
            &linked,
            options) != SPV_SUCCESS)
        return 0;
    const auto bytes = linked.size() * sizeof(uint32_t);
    auto* copy = static_cast<uint32_t*>(malloc(bytes));
    if (!copy)
        return 0;
    memcpy(copy, linked.data(), bytes);
    request->linkResult = copy;
    request->linkResultSize = linked.size();
    return 1;
}

void freeLinkResult(const uint32_t* words)
{
    free(const_cast<uint32_t*>(words));
}

int compileSpirv(glslang_CompileRequest_1_3* request)
{
    switch (request->action)
    {
    case GLSLANG_ACTION_OPTIMIZE_SPIRV:
        return optimizeSpirv(request);
    case GLSLANG_ACTION_DISSASSEMBLE_SPIRV:
        {
            const auto* words = static_cast<const uint32_t*>(request->inputBegin);
            const auto count = (static_cast<const char*>(request->inputEnd) -
                                static_cast<const char*>(request->inputBegin)) /
                               sizeof(uint32_t);
            spvtools::SpirvTools tools(getSpirvEnvironment(words));
            std::string text;
            if (!tools.Disassemble(words, count, &text))
                return -1;
            request->outputFunc(text.data(), text.size(), request->outputUserData);
            return 0;
        }
    default:
        return -1;
    }
}

class BuiltinSpirvToolsLibrary : public ISlangSharedLibrary
{
public:
    SLANG_IUNKNOWN_QUERY_INTERFACE
    SLANG_NO_THROW uint32_t SLANG_MCALL addRef() SLANG_OVERRIDE { return 1; }
    SLANG_NO_THROW uint32_t SLANG_MCALL release() SLANG_OVERRIDE { return 1; }
    SLANG_NO_THROW void* SLANG_MCALL castAs(const SlangUUID& guid) SLANG_OVERRIDE
    {
        return getInterface(guid);
    }
    ISlangUnknown* getInterface(const Guid& guid)
    {
        return guid == ISlangUnknown::getTypeGuid() || guid == ISlangCastable::getTypeGuid() ||
                       guid == ISlangSharedLibrary::getTypeGuid()
                   ? static_cast<ISlangSharedLibrary*>(this)
                   : nullptr;
    }
    SLANG_NO_THROW void* SLANG_MCALL findSymbolAddressByName(const char* name) SLANG_OVERRIDE
    {
        const struct
        {
            const char* name;
            SlangFuncPtr function;
        } functions[] = {
            {"glslang_compile_1_3", reinterpret_cast<SlangFuncPtr>(compileSpirv)},
            {"glslang_validateSPIRV", reinterpret_cast<SlangFuncPtr>(validateSpirv)},
            {"glslang_disassembleSPIRV", reinterpret_cast<SlangFuncPtr>(disassembleSpirv)},
            {"glslang_disassembleSPIRVWithResult",
             reinterpret_cast<SlangFuncPtr>(disassembleSpirvWithResult)},
            {"glslang_freeDisassembly", reinterpret_cast<SlangFuncPtr>(freeDisassembly)},
            {"glslang_linkSPIRV", reinterpret_cast<SlangFuncPtr>(linkSpirv)},
            {"glslang_freeLinkResult", reinterpret_cast<SlangFuncPtr>(freeLinkResult)},
        };
        for (const auto& function : functions)
            if (strcmp(name, function.name) == 0)
                return reinterpret_cast<void*>(function.function);
        return nullptr;
    }
};
} // namespace

ISlangSharedLibrary* getBuiltinSpirvToolsLibrary()
{
    static BuiltinSpirvToolsLibrary library;
    return &library;
}
} // namespace Slang

#endif
