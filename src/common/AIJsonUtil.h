/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 */

#ifndef SURGE_SRC_COMMON_AIJSONUTIL_H
#define SURGE_SRC_COMMON_AIJSONUTIL_H

#include "AIModelProvider.h"
#include "juce_core/juce_core.h"

#include <regex>
#include <string>
#include <vector>

// Header-only helpers shared by every DeepSynth AI provider (Claude, Gemini,
// OpenAI). Kept header-only on purpose so it needs no CMakeLists entry.
//
// The provider clients used to (a) hand-escape request strings with chained
// String::replace calls and (b) scan raw HTTP bodies for `"text":` with manual
// quote/backslash state machines. Both broke on perfectly valid API responses:
// unicode escapes (\uXXXX), multiple content blocks, or any `"text"` key that
// appeared earlier in the JSON (e.g. inside an error object or usage metadata).
// These helpers replace both with JUCE's real JSON parser.
namespace Surge
{
namespace AI
{
namespace JsonUtil
{

// Escape an arbitrary UTF-8 string into a JSON string literal *including* the
// surrounding double quotes, using JUCE's serializer. Correct for every control
// character, quote and backslash, so callers can splice it straight into a
// request body.
inline juce::String toJsonStringLiteral(const std::string &raw)
{
    return juce::JSON::toString(juce::var(juce::String(raw)), true);
}

// Walk a chain of object keys / array indices through a parsed juce::var,
// returning the string at the end or an empty String if any hop is missing.
// Keys are strings; array indices are passed as ints wrapped in juce::var.
inline juce::String getStringAtPath(const juce::var &root, const std::vector<juce::var> &path)
{
    juce::var node = root;
    for (const auto &step : path)
    {
        if (step.isInt() || step.isInt64() || step.isDouble())
        {
            if (auto *arr = node.getArray())
            {
                int idx = static_cast<int>(step);
                if (idx < 0 || idx >= arr->size())
                    return {};
                node = (*arr)[idx];
            }
            else
            {
                return {};
            }
        }
        else
        {
            if (!node.isObject())
                return {};
            node = node.getProperty(juce::Identifier(step.toString()), juce::var());
        }
    }
    return node.isString() ? node.toString() : juce::String();
}

// Pull a human-readable error message out of an API error body, falling back to
// the HTTP status code. Understands the common `{"error":{"message":...}}` and
// `{"error":{"status":...}}` shapes used by Anthropic, Google and OpenAI.
inline std::string errorMessageFromBody(const juce::String &body, int statusCode)
{
    juce::var parsed = juce::JSON::parse(body);
    if (parsed.isObject())
    {
        juce::var err = parsed.getProperty("error", juce::var());
        if (err.isObject())
        {
            for (const char *key : {"message", "status", "type", "code"})
            {
                juce::var v = err.getProperty(key, juce::var());
                if (v.isString() && v.toString().isNotEmpty())
                    return "API error (status " + std::to_string(statusCode) +
                           "): " + v.toString().toStdString();
            }
        }
        if (err.isString() && err.toString().isNotEmpty())
            return "API error (status " + std::to_string(statusCode) +
                   "): " + err.toString().toStdString();
    }
    // No structured error: return the status plus a truncated raw body so the
    // user still gets a diagnostic, but never a multi-kilobyte dump in the UI.
    std::string tail = body.substring(0, 240).toStdString();
    return "API error (status " + std::to_string(statusCode) + ")" +
           (tail.empty() ? "" : ": " + tail);
}

// Parse the model's "- name: value" (or "name = value") parameter list into
// modifications. Tolerant of a leading bullet (-, *, •), of `:` or `=`, and of
// scientific notation, but still requires a numeric value so prose lines are
// ignored. Parameter names may contain spaces (Surge display names like
// "A Filter 1 Cutoff") and are trimmed.
inline std::vector<PatchModification> parseParameterList(const std::string &responseText)
{
    std::vector<PatchModification> mods;
    static const std::regex paramRegex(
        R"((?:^|\n)[ \t>*-]*([A-Za-z][A-Za-z0-9_ ]*?)\s*[:=]\s*([-+]?(?:[0-9]*\.[0-9]+|[0-9]+\.?)(?:[eE][-+]?[0-9]+)?))");
    for (std::sregex_iterator it(responseText.begin(), responseText.end(), paramRegex), end;
         it != end; ++it)
    {
        const std::smatch &match = *it;
        if (match.size() < 3)
            continue;

        std::string name = match[1].str();
        name.erase(0, name.find_first_not_of(" \t\r\n"));
        name.erase(name.find_last_not_of(" \t\r\n") + 1);
        if (name.empty())
            continue;

        try
        {
            PatchModification mod;
            mod.parameterName = name;
            mod.value = std::stof(match[2].str());
            mods.push_back(mod);
        }
        catch (...)
        {
            // Non-finite / out-of-range literal: skip rather than abort the batch.
        }
    }
    return mods;
}

} // namespace JsonUtil
} // namespace AI
} // namespace Surge

#endif // SURGE_SRC_COMMON_AIJSONUTIL_H
