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

#include "ClaudeAPIClient.h"
#include "AIDebug.h"
#include "AIJsonUtil.h"
#include "SurgeStorage.h"
#include "UserDefaults.h"
#include "PatchVectorDB.h"
#include "juce_events/juce_events.h"
#include <sstream>
#include <algorithm>
#include <vector>

using namespace Surge::Claude;
using Surge::AI::AIResponse;
using Surge::AI::PatchModification;

// Anthropic model IDs. Aliases (no date suffix) resolve to the newest snapshot
// of that model, so these keep working as new snapshots ship. Update here to
// pin a dated snapshot or add a newer model.
static const char *claudeModelId(Surge::AI::ModelType type)
{
    switch (type)
    {
    case Surge::AI::ModelType::CLAUDE_OPUS_4_8:
        return "claude-opus-4-8";
    case Surge::AI::ModelType::CLAUDE_HAIKU_4_5:
        return "claude-haiku-4-5";
    case Surge::AI::ModelType::CLAUDE_SONNET_5:
    default:
        return "claude-sonnet-5";
    }
}
static constexpr int CLAUDE_MAX_TOKENS = 2048;

APIClient::APIClient(SurgeStorage *storage, Surge::AI::ModelType type)
    : storage(storage), currentModel(type)
{
    apiKey = Surge::Storage::getUserDefaultValue(storage, Surge::Storage::ClaudeAPIKey, "");
}

APIClient::~APIClient() = default;

void APIClient::setAPIKey(const std::string &key)
{
    apiKey = key;
    Surge::Storage::updateUserDefaultValue(storage, Surge::Storage::ClaudeAPIKey, key);
}

std::string APIClient::getAPIKey() const { return apiKey; }

bool APIClient::isAPIKeyValid() const { return !apiKey.empty(); }

void APIClient::setVectorDatabase(std::shared_ptr<Surge::PatchDB::VectorDatabase> db)
{
    vectorDatabase = db;
}

void APIClient::generatePatch(const std::string &prompt,
                              std::function<void(const AIResponse &)> callback)
{
    std::string enhancedPrompt = prompt;
    if (vectorDatabase)
    {
        enhancedPrompt = generateEnhancedPrompt(prompt);
    }

    std::string context = R"(
You are a Surge XT synthesizer patch designer expert.
Create a patch based on the user's description.

CRITICAL OUTPUT FORMAT:
You must output a list of parameter changes.
Each line must start with a dash.
Format: "- parameter_name: value"

Example:
PARAMETERS:
- filter1_cutoff: 0.5
- osc1_type: 2

Available parameters (use exact names):
- osc1_type, osc2_type, osc3_type (0-15)
- osc1_pitch, osc2_pitch (-60.0 to 60.0)
- filter1_type, filter2_type (0-12)
- filter1_cutoff, filter2_cutoff (0.0 to 1.0)
- filter1_resonance (0.0 to 1.0)
- amp_attack, amp_decay, amp_sustain, amp_release (0.0 to 1.0)
- filter_attack, filter_decay, filter_sustain, filter_release (0.0 to 1.0)
- volume (0.0 to 1.0)

Provide 5-10 parameter changes to achieve the sound.
User request: )";

    makeAPIRequest(enhancedPrompt, context, callback);
}

void APIClient::modifyPatch(const std::string &prompt, const std::string &currentPatchXML,
                            std::function<void(const AIResponse &)> callback)
{
    std::string context = R"(
You are modifying an existing Surge XT patch.
Suggest parameter changes based on the user's request.

CRITICAL OUTPUT FORMAT:
PARAMETERS:
- parameter_name: value

Current patch state:
)";
    // Pass the current patch so modifications are relative to what is loaded.
    context += currentPatchXML;
    context += "\n\nUser modification request: ";

    makeAPIRequest(prompt, context, callback);
}

void APIClient::makeAPIRequest(const std::string &prompt, const std::string &context,
                               std::function<void(const AIResponse &)> callback)
{
    DSTRACE << "DEBUG: Claude makeAPIRequest" << std::endl;

    if (!isAPIKeyValid())
    {
        AIResponse response;
        response.success = false;
        response.errorMessage = "Invalid API key. Please set a valid Claude API key in settings.";
        callback(response);
        return;
    }

    juce::String model = claudeModelId(currentModel);

    // Build the request with the JSON serializer so the prompt is escaped
    // correctly regardless of its contents.
    juce::String fullPrompt = juce::String(context) + juce::String(prompt);
    juce::String jsonRequest =
        "{\"model\":\"" + model + "\",\"max_tokens\":" + juce::String(CLAUDE_MAX_TOKENS) +
        ",\"messages\":[{\"role\":\"user\",\"content\":" +
        Surge::AI::JsonUtil::toJsonStringLiteral(fullPrompt.toStdString()) + "}]}";

    juce::URL url("https://api.anthropic.com/v1/messages");
    juce::String apiKeyCopy = apiKey;

    juce::Thread::launch([jsonRequest, callback, url, apiKeyCopy]() {
        AIResponse response;

        try
        {
            juce::URL requestUrl = url.withPOSTData(jsonRequest);

            juce::String headersString = "x-api-key: " + apiKeyCopy + "\r\n" +
                                         "anthropic-version: 2023-06-01\r\n" +
                                         "content-type: application/json\r\n";

            int statusCode = 0;
            juce::StringPairArray responseHeaders;
            auto stream = requestUrl.createInputStream(false, nullptr, nullptr, headersString,
                                                       30000, &responseHeaders, &statusCode);

            if (stream != nullptr)
            {
                juce::String responseString = stream->readEntireStreamAsString();

                if (statusCode == 200)
                {
                    // Messages API: { "content": [ { "type": "text", "text": "..." } ] }
                    juce::var parsed = juce::JSON::parse(responseString);
                    juce::String textContent =
                        Surge::AI::JsonUtil::getStringAtPath(parsed, {"content", 0, "text"});

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
                        response.errorMessage = "Failed to parse Claude response (no text content)";
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
                response.errorMessage = "Connection failed";
            }
        }
        catch (const std::exception &e)
        {
            response.success = false;
            response.errorMessage = e.what();
        }

        juce::MessageManager::callAsync([callback, response]() { callback(response); });
    });
}

std::vector<PatchModification> APIClient::extractModifications(const std::string &responseText)
{
    return Surge::AI::JsonUtil::parseParameterList(responseText);
}

std::string APIClient::generateEnhancedPrompt(const std::string &userPrompt)
{
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
