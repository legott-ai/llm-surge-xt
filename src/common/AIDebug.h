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

#ifndef SURGE_SRC_COMMON_AIDEBUG_H
#define SURGE_SRC_COMMON_AIDEBUG_H

#include <cstdlib>
#include <iostream>

// The DeepSynth AI code was written with std::cout << "DEBUG: ..." tracing on
// every parameter set, HTTP request and response. That is useful while
// developing but floods the terminal (and leaks prompt/response text) in a
// normal session. DSTRACE routes those statements through a runtime gate so the
// logging is off by default and can be re-enabled without a rebuild by setting
// the DEEPSYNTH_DEBUG environment variable to a non-empty, non-"0" value.
namespace Surge
{
namespace AI
{

inline bool debugLoggingEnabled()
{
    // Evaluated once; cheap and thread-safe under C++11 static-init rules.
    static const bool enabled = []() {
        const char *v = std::getenv("DEEPSYNTH_DEBUG");
        return v != nullptr && v[0] != '\0' && !(v[0] == '0' && v[1] == '\0');
    }();
    return enabled;
}

} // namespace AI
} // namespace Surge

// Drop-in stream replacement for std::cout in DeepSynth diagnostics. The
// if/else form is intentional: it keeps DSTRACE safe to use as a statement in
// front of a trailing `else` and evaluates the streamed arguments only when
// logging is enabled.
#define DSTRACE                                                                                    \
    if (!::Surge::AI::debugLoggingEnabled())                                                       \
    {                                                                                              \
    }                                                                                              \
    else                                                                                           \
        std::cout

#endif // SURGE_SRC_COMMON_AIDEBUG_H
