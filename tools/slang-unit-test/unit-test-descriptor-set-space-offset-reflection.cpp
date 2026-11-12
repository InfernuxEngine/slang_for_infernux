// unit-test-descriptor-set-space-offset-reflection.cpp

#include "slang-com-ptr.h"
#include "slang.h"
#include "unit-test/slang-unit-test.h"

#include <stdio.h>
#include <stdlib.h>

using namespace Slang;

// Verify that `TypeLayoutReflection::getDescriptorSetSpaceOffset`
// returns the user's actual `space=N` for HLSL `register(_, spaceN)`
// declarations (and equivalently for Vulkan `[[vk::binding(_, N)]]`).
//
// A previous bug left `DescriptorSetInfo::spaceOffset` default-
// initialized to 0 when descriptor sets were created in
// `_findOrAddDescriptorSet`. `getDescriptorSetSpaceOffset` therefore
// returned 0 for every set regardless of the user's declaration,
// causing slang-rhi backends (D3D12 root-signature builder, Vulkan
// descriptor-set allocator) to lay out their descriptor tables wrong
// for any non-zero space — D3D12 rejected the layout at root-
// signature creation; Vulkan silently mis-bound the buffer.
//
// The high-level reflection JSON path (`parameters[].binding.space`)
// goes through `VariableLayoutReflection::getOffset` and was already
// correct, so this regression specifically targets the descriptor-set
// path used by slang-rhi.
//
// See: https://github.com/shader-slang/slang/issues/10959
SLANG_UNIT_TEST(descriptorSetSpaceOffsetReflection)
{
    const char* userSource = R"(
        RWStructuredBuffer<uint> A : register(u0);
        RWStructuredBuffer<uint> B : register(u7, space3);

        [Shader("compute")]
        [NumThreads(64, 1, 1)]
        void computeMain(uint3 tid : SV_DispatchThreadID)
        {
            A[tid.x] = A[tid.x] + 1;
            B[tid.x] = B[tid.x] + 1;
        }
    )";

    ComPtr<slang::IGlobalSession> globalSession;
    SLANG_CHECK(slang_createGlobalSession(SLANG_API_VERSION, globalSession.writeRef()) == SLANG_OK);

    slang::TargetDesc targetDesc = {};
    targetDesc.format = SLANG_SPIRV;
    targetDesc.profile = globalSession->findProfile("spirv_1_5");

    slang::SessionDesc sessionDesc = {};
    sessionDesc.targetCount = 1;
    sessionDesc.targets = &targetDesc;

    ComPtr<slang::ISession> session;
    SLANG_CHECK(globalSession->createSession(sessionDesc, session.writeRef()) == SLANG_OK);

    ComPtr<slang::IBlob> diagnosticBlob;
    auto module = session->loadModuleFromSourceString(
        "test",
        "test.slang",
        userSource,
        diagnosticBlob.writeRef());
    SLANG_CHECK(module != nullptr);

    ComPtr<slang::IEntryPoint> entryPoint;
    module->findEntryPointByName("computeMain", entryPoint.writeRef());
    SLANG_CHECK(entryPoint != nullptr);

    ComPtr<slang::IComponentType> compositeProgram;
    slang::IComponentType* components[] = {module, entryPoint.get()};
    session->createCompositeComponentType(
        components,
        2,
        compositeProgram.writeRef(),
        diagnosticBlob.writeRef());
    SLANG_CHECK(compositeProgram != nullptr);

    ComPtr<slang::IComponentType> linkedProgram;
    compositeProgram->link(linkedProgram.writeRef(), diagnosticBlob.writeRef());
    SLANG_CHECK(linkedProgram != nullptr);

    auto programLayout = linkedProgram->getLayout();
    SLANG_CHECK(programLayout != nullptr);

    auto globalParamsLayout = programLayout->getGlobalParamsVarLayout()->getTypeLayout();
    SLANG_CHECK(globalParamsLayout != nullptr);

    // Two distinct logical descriptor sets are expected: one for A
    // (space=0) and one for B (space=3).
    SlangInt setCount = globalParamsLayout->getDescriptorSetCount();
    SLANG_CHECK(setCount == 2);

    bool seenSpace0 = false;
    bool seenSpace3 = false;
    for (SlangInt i = 0; i < setCount; ++i)
    {
        SlangInt spaceOffset = globalParamsLayout->getDescriptorSetSpaceOffset(i);
        if (spaceOffset == 0)
            seenSpace0 = true;
        if (spaceOffset == 3)
            seenSpace3 = true;
    }
    SLANG_CHECK(seenSpace0);
    SLANG_CHECK(seenSpace3);
}

SLANG_UNIT_TEST(combinedSamplerDescriptorRanges)
{
    const char* source = R"(
        [[vk::binding(2, 3)]] Sampler2D<float4> single;
        [[vk::binding(8, 3)]] Sampler2D<float4> pair[2];
        [shader("fragment")]
        float4 main(float2 uv : TEXCOORD0) : SV_Target0
        {
            return single.Sample(uv) + pair[1].Sample(uv);
        }
    )";
    ComPtr<slang::IGlobalSession> global;
    SLANG_CHECK(slang_createGlobalSession(SLANG_API_VERSION, global.writeRef()) == SLANG_OK);
    for (auto format : {SLANG_SPIRV, SLANG_WGSL})
    {
        slang::TargetDesc target = {};
        target.format = format;
        target.profile = global->findProfile("spirv_1_5");
        slang::SessionDesc description = {};
        description.targetCount = 1;
        description.targets = &target;
        ComPtr<slang::ISession> session;
        SLANG_CHECK(global->createSession(description, session.writeRef()) == SLANG_OK);
        ComPtr<slang::IBlob> diagnostics;
        auto module = session->loadModuleFromSourceString(
            "samplers",
            "samplers.slang",
            source,
            diagnostics.writeRef());
        SLANG_CHECK(module != nullptr);
        auto layout = module->getLayout()->getGlobalParamsTypeLayout();
        SLANG_CHECK(layout->getBindingRangeCount() == 2);
        for (SlangInt index = 0; index < 2; ++index)
        {
            auto set = layout->getBindingRangeDescriptorSetIndex(index);
            auto first = layout->getBindingRangeFirstDescriptorRangeIndex(index);
            SLANG_CHECK(layout->getDescriptorSetSpaceOffset(set) == 3);
            SLANG_CHECK(
                layout->getBindingRangeType(index) == slang::BindingType::CombinedTextureSampler);
            auto split = format == SLANG_WGSL;
            SLANG_CHECK(layout->getBindingRangeDescriptorRangeCount(index) == (split ? 2 : 1));
            SLANG_CHECK(
                layout->getDescriptorSetDescriptorRangeType(set, first) ==
                (split ? slang::BindingType::Texture : slang::BindingType::CombinedTextureSampler));
            SLANG_CHECK(
                layout->getDescriptorSetDescriptorRangeDescriptorCount(set, first) == index + 1);
            SLANG_CHECK(
                layout->getDescriptorSetDescriptorRangeIndexOffset(set, first) == (index ? 8 : 2));
            if (split)
            {
                SLANG_CHECK(
                    layout->getDescriptorSetDescriptorRangeType(set, first + 1) ==
                    slang::BindingType::Sampler);
                SLANG_CHECK(
                    layout->getDescriptorSetDescriptorRangeIndexOffset(set, first + 1) ==
                    (index ? 9 : 3));
                SLANG_CHECK(
                    layout->getDescriptorSetDescriptorRangeDescriptorCount(set, first + 1) ==
                    index + 1);
            }
        }
    }
}
