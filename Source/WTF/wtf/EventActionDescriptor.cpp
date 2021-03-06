/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include <climits>
#include <sstream>
#include <algorithm>

#include "EventActionDescriptor.h"

namespace WTF {

EventActionDescriptor EventActionDescriptor::null;

EventActionDescriptor::EventActionDescriptor(EventActionCategory category, const std::string& type, const std::string& params)
    : m_category(category)
    , m_type(type)
    , m_params(params)
    , m_isNull(false)
    , m_patched(false)
{
}

EventActionDescriptor::EventActionDescriptor()
    : m_isNull(true)
    , m_patched(false)
{
}

bool EventActionDescriptor::operator==(const EventActionDescriptor& other) const
{
    return m_category == other.m_category && m_type == other.m_type && m_params == other.m_params;
}

bool EventActionDescriptor::operator!=(const EventActionDescriptor& other) const
{
    return !operator==(other);
}

std::string EventActionDescriptor::toString() const
{
    if (m_full_cache.empty()) {
        std::stringstream result;
        result << m_category << "-" << m_type << "(" << m_params << ")";

        m_full_cache = result.str();
    }

    return m_full_cache;
}

std::string EventActionDescriptor::toUnpatchedString() const {
    return isPatched() ? m_unpatchedString : toString();
}

std::string EventActionDescriptor::serialize() const
{
    return toString();
}

EventActionDescriptor EventActionDescriptor::deserialize(const std::string& raw)
{
    size_t typeStartPos = raw.find('-');
    assert(typeStartPos != std::string::npos);
    size_t typeEndPos = raw.find('(');
    assert(typeEndPos != std::string::npos);

    assert(typeStartPos < typeEndPos);

    return EventActionDescriptor(
                (EventActionCategory)atoi((raw.substr(0, typeStartPos)).c_str()),
                raw.substr(typeStartPos+1, typeEndPos-typeStartPos-1),
                raw.substr(typeEndPos+1, raw.size()-typeEndPos-2));
}

std::string EventActionDescriptor::getParameter(unsigned int number) const
{
    size_t start = 0; // start index of param
    size_t end = m_params.find(','); // index just after the param

    if (end == std::string::npos) {
       end = m_params.size(); // special case if there is only one argument
    }

    for (int i = 0; i < number; i++) {
        assert(end != m_params.size()); // indexing into non-existing param

        start = end + 1; // end points at the "," just before the start position
        end = m_params.find(',', start);

        if (end == std::string::npos) {
           end = m_params.size();
        }
    }

    return m_params.substr(start, end-start);
}

void EventActionDescriptor::patchParameter(unsigned int number, const std::string& value) const
{
    m_unpatchedString = toString();

    size_t start = 0; // start index of param
    size_t end = m_params.find(','); // index just after the param

    if (end == std::string::npos) {
       end = m_params.size(); // special case if there is only one argument
    }

    for (int i = 0; i < number; i++) {
        assert(end != m_params.size()); // indexing into non-existing param

        start = end + 1; // end points at the "," just before the start position
        end = m_params.find(',', start);

        if (end == std::string::npos) {
           end = m_params.size();
        }
    }

    m_params = m_params.substr(0, start) + value + m_params.substr(end);
    m_patched = true;
    m_full_cache = std::string();
}

std::string EventActionDescriptor::escapeParam(const std::string& param)
{
    std::string newParam = param;
    std::replace(newParam.begin(), newParam.end(), ',', '.');

    return newParam;
}

}
