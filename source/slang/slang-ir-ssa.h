// slang-ir-ssa.h
#pragma once

namespace Slang
{
struct IRModule;
// Optimize local data after source diagnostics and differentiation.
bool normalizeLocalStructStores(IRModule* module);
struct IRGlobalValueWithCode;
struct IRInst;

bool constructSSA(IRModule* module, IRGlobalValueWithCode* globalVal);
bool constructSSA(IRModule* module);
bool constructSSA(IRInst* globalVal);
} // namespace Slang
