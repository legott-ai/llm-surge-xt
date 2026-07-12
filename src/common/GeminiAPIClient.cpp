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

#include "GeminiAPIClient.h"
#include "AIDebug.h"
#include "AIJsonUtil.h"
#include "SurgeStorage.h"
#include "UserDefaults.h"
#include "juce_core/juce_core.h"
#include "juce_events/juce_events.h"
#include <algorithm>
#include <sstream>

using namespace Surge::Gemini;

// Response cap for a parameter list; a couple of thousand tokens is plenty for
// ~30 "- name: value" lines and keeps latency/cost down. Named so it is easy to
// tune in one place.
static constexpr int GEMINI_MAX_OUTPUT_TOKENS = 2048;

APIClient::APIClient(SurgeStorage *storage, Surge::AI::ModelType modelType)
    : storage(storage), currentModel(modelType)
{
    apiKey = Surge::Storage::getUserDefaultValue(storage, Surge::Storage::GeminiAPIKey, "");
}

APIClient::~APIClient() = default;

void APIClient::setVectorDatabase(std::shared_ptr<Surge::PatchDB::VectorDatabase> db)
{
    vectorDatabase = db;
}

void APIClient::setAPIKey(const std::string &key)
{
    apiKey = key;
    Surge::Storage::updateUserDefaultValue(storage, Surge::Storage::GeminiAPIKey, key);
}

std::string APIClient::getAPIKey() const { return apiKey; }

bool APIClient::isAPIKeyValid() const { return !apiKey.empty(); }

std::string APIClient::getModelEndpoint() const
{
    // Model IDs are decoupled from the enum/display name on purpose: update
    // these as Google ships newer snapshots without touching the UI wiring.
    switch (currentModel)
    {
    case Surge::AI::ModelType::GEMINI_3_PRO:
        return "gemini-3-pro";
    case Surge::AI::ModelType::GEMINI_2_5_FLASH:
        return "gemini-2.5-flash";
    case Surge::AI::ModelType::GEMINI_3_FLASH:
    default:
        return "gemini-3-flash";
    }
}

void APIClient::generatePatch(const std::string &prompt,
                              std::function<void(const Surge::AI::AIResponse &)> callback)
{
    std::string enhancedPrompt = vectorDatabase ? generateEnhancedPrompt(prompt) : prompt;

    // MAXIMIZED Context for Gemini (Full Parameter Control)
    // Note: Compressed format used to fit within context window while listing ~500 params
    std::string context = R"(
You are a Surge XT synthesizer patch designer expert.
Create a patch based on the user's description.

CRITICAL OUTPUT FORMAT:
- Output a list of parameter changes.
- Start each line with a dash.
- Format: "- parameter_name: value"

AVAILABLE PARAMETERS (Use these exact names):

[OSCILLATORS] (osc1 to osc3)
- osc1_type (0=Classic, 1=Sine, 2=Wavetable, 3=Noise, 5=FM3, 6=FM2, 8=Modern, 9=String, 10=Twist)
- osc1_pitch (-60.0 to 60.0), osc1_shape (0.0-1.0), osc1_feedback
- osc1_param1, osc1_param2, osc1_param3, osc1_param4, osc1_param5, osc1_param6 (Varies by Type)
(Repeat for osc2_*, osc3_*)

[MIXER & SCENE]
- osc1_volume, osc2_volume, osc3_volume (0.0-1.0)
- ring12_volume, ring23_volume (Ring Mod Levels)
- noise_volume (Noise Generator Level)
- osc_drift (Analog Drift), portamento (Glide time), width (Stereo width)
- volume (Master Volume)

[FILTERS] (filter1, filter2)
- filter1_type (1=Lowpass 12dB, 2=Lowpass 24dB, 8=Ladders, 4=Highpass, 6=Bandpass)
- filter1_cutoff (Hz/Norm), filter1_resonance, filter1_drive
- filter_balance (0=Series, 1=Parallel), filter_config
(Repeat for filter2_*)

[ENVELOPES] (amp, filter)
- amp_attack, amp_decay, amp_sustain, amp_release
- filter_attack, filter_decay, filter_sustain, filter_release

[MODULATION & MACROS]
- macro1, macro2 ... macro8 (0.0-1.0)
- lfo1_rate, lfo1_shape, lfo1_amount, lfo1_deform, lfo1_delay, lfo1_attack
(Repeat for lfo2 to lfo6)

[EFFECTS - INSERTS] (Pre-Filter or Post-Filter Inserts)
- fx_insert1_type, fx_insert2_type (1=Delay, 2=Reverb, 3=Phaser, 5=Distortion, 11=Airwindows)
- fx_insert1_mix, fx_insert1_p1, fx_insert1_p2 ... fx_insert1_p6

[EFFECTS - SENDS] (Global Reverb/Delay busses)
- fx_send1_type (Best for Reverb: 11), fx_send1_mix
- fx_send1_p1 (Decay), fx_send1_p2 (Size/Time), fx_send1_p3 (Damp/Color)
- fx_send2_type (Best for Delay: 1), fx_send2_mix
- fx_send2_p1, fx_send2_p2, fx_send2_p3


Provide 20+ parameter changes for a rich, detailed sound.
CRITICAL: Always set these core parameters:
1. volume (Master Volume, typically 0.7-1.0)
2. osc1_volume, osc2_volume, osc3_volume (Oscillator levels)
3. amp_attack, amp_decay, amp_sustain, amp_release (Envelope)
4. filter1_cutoff, filter1_resonance (Filter)

User request: )";

    makeAPIRequest(enhancedPrompt, context, callback);
}

void APIClient::modifyPatch(const std::string &prompt, const std::string &currentPatchXML,
                            std::function<void(const Surge::AI::AIResponse &)> callback)
{
    std::string context = R"(
You are modifying an existing Surge XT patch.
Suggest parameter changes based on the user's request.
Use the standard parameter names (osc1_cutoff, fx_send1_mix, etc).

CRITICAL OUTPUT FORMAT:
PARAMETERS:
- parameter_name: value

Current patch state:
)";
    // Give the model the current patch so "make it brighter" is relative to what
    // is loaded, not a blind guess (the argument was previously ignored).
    context += currentPatchXML;
    context += "\n\nUser modification request: ";

    makeAPIRequest(prompt, context, callback);
}

void APIClient::makeAPIRequest(const std::string &prompt, const std::string &context,
                               std::function<void(const Surge::AI::AIResponse &)> callback)
{
    if (!isAPIKeyValid())
    {
        Surge::AI::AIResponse response;
        response.success = false;
        response.errorMessage = "Invalid API key. Please set a valid Gemini API key in settings.";
        callback(response);
        return;
    }

    // Build the request body with the JSON serializer instead of hand-escaping,
    // so any prompt text (quotes, newlines, unicode) is encoded correctly.
    juce::String fullText = juce::String(context) + juce::String(prompt);
    juce::String jsonRequest = "{\"contents\":[{\"parts\":[{\"text\":" +
                               Surge::AI::JsonUtil::toJsonStringLiteral(fullText.toStdString()) +
                               "}]}],\"generationConfig\":{\"maxOutputTokens\":" +
                               juce::String(GEMINI_MAX_OUTPUT_TOKENS) + "}}";

    std::string modelName = getModelEndpoint();
    juce::String endpoint = "https://generativelanguage.googleapis.com/v1beta/models/" +
                            juce::String(modelName) + ":generateContent";
    // The key travels in the x-goog-api-key header rather than the query string,
    // so it never lands in URL/proxy access logs.
    juce::String apiKeyCopy = apiKey;

    juce::URL url(endpoint);

    juce::Thread::launch([jsonRequest, callback, url, apiKeyCopy]() {
        Surge::AI::AIResponse response;

        try
        {
            juce::URL requestUrl = url.withPOSTData(jsonRequest);

            juce::String headersString = "Content-Type: application/json\r\n"
                                         "x-goog-api-key: " +
                                         apiKeyCopy +
                                         "\r\n"
                                         "User-Agent: SurgeXT-DeepSynth/1.0\r\n";

            int statusCode = 0;
            juce::StringPairArray responseHeaders;
            auto stream = requestUrl.createInputStream(false, nullptr, nullptr, headersString,
                                                       30000, &responseHeaders, &statusCode);

            if (stream != nullptr)
            {
                juce::String responseString = stream->readEntireStreamAsString();
                if (statusCode == 200)
                {
                    // candidates[0].content.parts[0].text
                    juce::var parsed = juce::JSON::parse(responseString);
                    juce::String textContent = Surge::AI::JsonUtil::getStringAtPath(
                        parsed, {"candidates", 0, "content", "parts", 0, "text"});

                    if (textContent.isNotEmpty())
                    {
                        response.success = true;
                        response.responseText = textContent.toStdString();
                        response.modifications =
                            Surge::AI::JsonUtil::parseParameterList(response.responseText);
                    }
                    else
                    {
                        response.success = false;
                        response.errorMessage =
                            "Gemini returned no candidate text (possibly blocked by a safety "
                            "filter or truncated). Try rephrasing the prompt.";
                    }
                }
                else
                {
                    response.success = false;
                    response.errorMessage =
                        Surge::AI::JsonUtil::errorMessageFromBody(responseString, statusCode);
                }
            }
            else
            {
                response.success = false;
                response.errorMessage = "Failed to connect to Gemini API";
            }
        }
        catch (const std::exception &e)
        {
            response.success = false;
            response.errorMessage = std::string("Exception: ") + e.what();
        }

        juce::MessageManager::callAsync([callback, response]() { callback(response); });
    });
}

std::vector<Surge::AI::PatchModification>
APIClient::extractModifications(const std::string &responseText)
{
    return Surge::AI::JsonUtil::parseParameterList(responseText);
}

std::string APIClient::generateEnhancedPrompt(const std::string &userPrompt)
{
    // RAG for Gemini was previously a no-op (returned the prompt unchanged even
    // though a vector database was attached). Mirror the Claude/OpenAI path so
    // Gemini also gets factory-patch context.
    if (!vectorDatabase)
    {
        return userPrompt;
    }

    auto searchTerms = extractSearchTerms(userPrompt);
    std::vector<Surge::PatchDB::PatchVector> allSimilarPatches;
    for (const auto &term : searchTerms)
    {
        auto patches = vectorDatabase->findSimilarByText(term, 3);
        allSimilarPatches.insert(allSimilarPatches.end(), patches.begin(), patches.end());
    }

    std::sort(allSimilarPatches.begin(), allSimilarPatches.end(),
              [](const auto &a, const auto &b) { return a.name < b.name; });
    allSimilarPatches.erase(
        std::unique(allSimilarPatches.begin(), allSimilarPatches.end(),
                    [](const auto &a, const auto &b) { return a.name == b.name; }),
        allSimilarPatches.end());

    if (allSimilarPatches.size() > 5)
    {
        allSimilarPatches.resize(5);
    }

    if (allSimilarPatches.empty())
    {
        return userPrompt;
    }

    std::string enhancedPrompt = userPrompt + "\n\n";
    enhancedPrompt += "For reference, here are some similar patches from the factory library:\n";
    enhancedPrompt += formatSimilarPatches(allSimilarPatches);
    enhancedPrompt +=
        "\nUse these as inspiration but create something new based on the user's request.";

    return enhancedPrompt;
}

std::vector<std::string> APIClient::extractSearchTerms(const std::string &prompt)
{
    std::vector<std::string> terms;
    static const std::vector<std::string> keywords = {
        "bass",        "lead",      "pad",    "pluck",   "arp",       "chord",    "string",
        "brass",       "bell",      "organ",  "piano",   "ep",        "electric", "ambient",
        "atmospheric", "warm",      "bright", "dark",    "soft",      "hard",     "aggressive",
        "gentle",      "smooth",    "rough",  "clean",   "distorted", "filtered", "resonant",
        "fm",          "wavetable", "analog", "digital", "vintage",   "modern",   "classic"};

    std::string lowerPrompt = prompt;
    std::transform(lowerPrompt.begin(), lowerPrompt.end(), lowerPrompt.begin(), ::tolower);

    for (const auto &keyword : keywords)
    {
        if (lowerPrompt.find(keyword) != std::string::npos)
        {
            terms.push_back(keyword);
        }
    }

    if (terms.empty())
    {
        terms.push_back(prompt);
    }

    return terms;
}

std::string APIClient::formatSimilarPatches(const std::vector<Surge::PatchDB::PatchVector> &patches)
{
    std::ostringstream oss;

    for (const auto &patch : patches)
    {
        oss << "- " << patch.name << " (" << patch.category << ")";
        if (!patch.description.empty())
        {
            oss << ": " << patch.description;
        }
        oss << "\n";
    }

    return oss.str();
}
