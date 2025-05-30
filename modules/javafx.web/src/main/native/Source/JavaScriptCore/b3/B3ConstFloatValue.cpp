/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "B3ConstFloatValue.h"

#if ENABLE(B3_JIT)

#include "B3ConstDoubleValue.h"
#include "B3ProcedureInlines.h"
#include "B3ValueInlines.h"

namespace JSC { namespace B3 {

ConstFloatValue::~ConstFloatValue() = default;

Value* ConstFloatValue::negConstant(Procedure& proc) const
{
    return proc.add<ConstFloatValue>(origin(), -m_value);
}

Value* ConstFloatValue::addConstant(Procedure& proc, int32_t other) const
{
    return proc.add<ConstFloatValue>(origin(), m_value + static_cast<float>(other));
}

Value* ConstFloatValue::addConstant(Procedure& proc, const Value* other) const
{
    if (!other->hasFloat())
        return nullptr;
    return proc.add<ConstFloatValue>(origin(), m_value + other->asFloat());
}

Value* ConstFloatValue::subConstant(Procedure& proc, const Value* other) const
{
    if (!other->hasFloat())
        return nullptr;
    return proc.add<ConstFloatValue>(origin(), m_value - other->asFloat());
}

Value* ConstFloatValue::mulConstant(Procedure& proc, const Value* other) const
{
    if (!other->hasFloat())
        return nullptr;
    return proc.add<ConstFloatValue>(origin(), m_value * other->asFloat());
}

Value* ConstFloatValue::bitAndConstant(Procedure& proc, const Value* other) const
{
    if (!other->hasFloat())
        return nullptr;
    float result = bitwise_cast<float>(bitwise_cast<uint32_t>(m_value) & bitwise_cast<uint32_t>(other->asFloat()));
    return proc.add<ConstFloatValue>(origin(), result);
}

Value* ConstFloatValue::bitOrConstant(Procedure& proc, const Value* other) const
{
    if (!other->hasFloat())
        return nullptr;
    float result = bitwise_cast<float>(bitwise_cast<uint32_t>(m_value) | bitwise_cast<uint32_t>(other->asFloat()));
    return proc.add<ConstFloatValue>(origin(), result);
}

Value* ConstFloatValue::bitXorConstant(Procedure& proc, const Value* other) const
{
    if (!other->hasFloat())
        return nullptr;
    float result = bitwise_cast<float>(bitwise_cast<uint32_t>(m_value) ^ bitwise_cast<uint32_t>(other->asFloat()));
    return proc.add<ConstFloatValue>(origin(), result);
}

Value* ConstFloatValue::bitwiseCastConstant(Procedure& proc) const
{
    return proc.add<Const32Value>(origin(), bitwise_cast<int32_t>(m_value));
}

Value* ConstFloatValue::floatToDoubleConstant(Procedure& proc) const
{
    return proc.add<ConstDoubleValue>(origin(), static_cast<double>(m_value));
}

Value* ConstFloatValue::absConstant(Procedure& proc) const
{
    return proc.add<ConstFloatValue>(origin(), static_cast<float>(std::abs(m_value)));
}

Value* ConstFloatValue::ceilConstant(Procedure& proc) const
{
    return proc.add<ConstFloatValue>(origin(), ceilf(m_value));
}

Value* ConstFloatValue::floorConstant(Procedure& proc) const
{
    return proc.add<ConstFloatValue>(origin(), floorf(m_value));
}

Value* ConstFloatValue::sqrtConstant(Procedure& proc) const
{
    return proc.add<ConstFloatValue>(origin(), static_cast<float>(sqrt(m_value)));
}

Value* ConstFloatValue::divConstant(Procedure& proc, const Value* other) const
{
    if (!other->hasFloat())
        return nullptr;
    return proc.add<ConstFloatValue>(origin(), m_value / other->asFloat());
}

Value* ConstFloatValue::fMinConstant(Procedure& proc, const Value* other) const
{
    if (!other->hasFloat())
        return nullptr;
    return proc.add<ConstFloatValue>(origin(), Math::fMin(m_value, other->asFloat()));
}

Value* ConstFloatValue::fMaxConstant(Procedure& proc, const Value* other) const
{
    if (!other->hasFloat())
        return nullptr;
    return proc.add<ConstFloatValue>(origin(), Math::fMax(m_value, other->asFloat()));
}

TriState ConstFloatValue::equalConstant(const Value* other) const
{
    if (!other->hasFloat())
        return TriState::Indeterminate;
    return triState(m_value == other->asFloat());
}

TriState ConstFloatValue::notEqualConstant(const Value* other) const
{
    if (!other->hasFloat())
        return TriState::Indeterminate;
    return triState(m_value != other->asFloat());
}

TriState ConstFloatValue::lessThanConstant(const Value* other) const
{
    if (!other->hasFloat())
        return TriState::Indeterminate;
    return triState(m_value < other->asFloat());
}

TriState ConstFloatValue::greaterThanConstant(const Value* other) const
{
    if (!other->hasFloat())
        return TriState::Indeterminate;
    return triState(m_value > other->asFloat());
}

TriState ConstFloatValue::lessEqualConstant(const Value* other) const
{
    if (!other->hasFloat())
        return TriState::Indeterminate;
    return triState(m_value <= other->asFloat());
}

TriState ConstFloatValue::greaterEqualConstant(const Value* other) const
{
    if (!other->hasFloat())
        return TriState::Indeterminate;
    return triState(m_value >= other->asFloat());
}

TriState ConstFloatValue::equalOrUnorderedConstant(const Value* other) const
{
    if (std::isnan(m_value))
        return TriState::True;

    if (!other->hasFloat())
        return TriState::Indeterminate;
    float otherValue = other->asFloat();
    return triState(std::isunordered(m_value, otherValue) || m_value == otherValue);
}

void ConstFloatValue::dumpMeta(CommaPrinter& comma, PrintStream& out) const
{
    out.print(comma);
    out.printf("%le(%u)", m_value, bitwise_cast<uint32_t>(m_value));
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
