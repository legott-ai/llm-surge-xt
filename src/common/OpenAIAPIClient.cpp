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

#include "OpenAIAPIClient.h"
#include "AIDebug.h"
#include "AIJsonUtil.h"
#include "SurgeStorage.h"
#include "UserDefaults.h"
#include "juce_core/juce_core.h"
#include "juce_events/juce_events.h"
#include <algorithm>
#include <sstream>

using namespace Surge::OpenAI;

APIClient::APIClient(SurgeStorage *storage, Surge::AI::ModelType modelType)
    : storage(storage), currentModel(modelType)
{
    apiKey = Surge::Storage::getUserDefaultValue(storage, Surge::Storage::OpenAIAPIKey, "");
}

APIClient::~APIClient() = default;

void APIClient::setVectorDatabase(std::shared_ptr<Surge::PatchDB::VectorDatabase> db)
{
    vectorDatabase = db;
}

void APIClient::setAPIKey(const std::string &key)
{
    apiKey = key;
    Surge::Storage::updateUserDefaultValue(storage, Surge::Storage::OpenAIAPIKey, key);
}

std::string APIClient::getAPIKey() const { return apiKey; }

bool APIClient::isAPIKeyValid() const { return !apiKey.empty(); }

std::string APIClient::getModelEndpoint() const
{
    // Update these strings as OpenAI ships newer models; they are decoupled from
    // the enum/display name so the UI wiring does not need to change.
    switch (currentModel)
    {
    case Surge::AI::ModelType::OPENAI_GPT5:
        return "gpt-5";
    case Surge::AI::ModelType::OPENAI_GPT5O:
    default:
        return "gpt-5o";
    }
}

void APIClient::generatePatch(const std::string &prompt,
                              std::function<void(const Surge::AI::AIResponse &)> callback)
{
    std::string enhancedPrompt = vectorDatabase ? generateEnhancedPrompt(prompt) : prompt;

    std::string context = R"(
You are a Surge XT synthesizer patch designer. Create a patch based on the user's description.

IMPORTANT: You MUST respond with EXACTLY this format (include the dash before each parameter):

PARAMETERS:
- filter1_cutoff: 0.5
- filter1_resonance: 0.3
- osc1_type: 2
- amp_attack: 0.1

Available parameters (use these exact names):
- osc1_type, osc2_type, osc3_type (integer 0-15)
- osc1_pitch, osc2_pitch, osc3_pitch (-60.0 to 60.0)
- filter1_type, filter2_type (integer 0-12)
- filter1_cutoff, filter2_cutoff (0.0 to 1.0)
- filter1_resonance, filter2_resonance (0.0 to 1.0)
- amp_attack, amp_decay, amp_sustain, amp_release (0.0 to 1.0)
- filter_attack, filter_decay, filter_sustain, filter_release (0.0 to 1.0)
- lfo1_rate, lfo2_rate (0.0 to 1.0)
- volume (0.0 to 1.0)

Provide 5-10 parameter changes. Use normalized values (0.0-1.0) for continuous parameters.

User request: )";

    makeAPIRequest(enhancedPrompt, context, callback);
}

void APIClient::modifyPatch(const std::string &prompt, const std::string &currentPatchXML,
                            std::function<void(const Surge::AI::AIResponse &)> callback)
{
    std::string context = R"(
You are modifying an existing Surge XT synthesizer patch.
Suggest specific parameter changes based on the user's request.

IMPORTANT: You MUST respond with EXACTLY this format (include the dash before each parameter):

PARAMETERS:
- filter1_cutoff: 0.5
- filter1_resonance: 0.3
- osc1_type: 2
- amp_attack: 0.1

Available parameters (use these exact names):
- osc1_type, osc2_type, osc3_type (integer 0-15)
- osc1_pitch, osc2_pitch, osc3_pitch (-60.0 to 60.0)
- filter1_type, filter2_type (integer 0-12)
- filter1_cutoff, filter2_cutoff (0.0 to 1.0)
- filter1_resonance, filter2_resonance (0.0 to 1.0)
- amp_attack, amp_decay, amp_sustain, amp_release (0.0 to 1.0)
- filter_attack, filter_decay, filter_sustain, filter_release (0.0 to 1.0)
- lfo1_rate, lfo2_rate (0.0 to 1.0)
- volume (0.0 to 1.0)

Current patch state:
)";
    context += currentPatchXML;
    context += "\n\nUser modification request: ";

    makeAPIRequest(prompt, context, callback);
}

void APIClient::makeAPIRequest(const std::string &prompt, const std::string &context,
                               std::function<void(const Surge::AI::AIResponse &)> callback)
{
    DSTRACE << "DEBUG: OpenAI makeAPIRequest" << std::endl;

    if (!isAPIKeyValid())
    {
        Surge::AI::AIResponse response;
        response.success = false;
        response.errorMessage = "Invalid API key. Please set a valid OpenAI API key in settings.";
        callback(response);
        return;
    }

    juce::String fullPrompt = juce::String(context) + juce::String(prompt);
    std::string modelName = getModelEndpoint();
    juce::String jsonRequest =
        "{\"model\":\"" + juce::String(modelName) +
        "\",\"messages\":[{\"role\":\"user\",\"content\":" +
        Surge::AI::JsonUtil::toJsonStringLiteral(fullPrompt.toStdString()) + "}]}";

    juce::URL url("https://api.openai.com/v1/chat/completions");
    juce::String apiKeyCopy = apiKey;

    juce::Thread::launch([jsonRequest, callback, url, apiKeyCopy]() {
        Surge::AI::AIResponse response;

        try
        {
            juce::URL requestUrl = url.withPOSTData(jsonRequest);

            juce::String headersString =
                "Content-Type: application/json\r\nAuthorization: Bearer " + apiKeyCopy + "\r\n";

            int statusCode = 0;
            juce::StringPairArray responseHeaders;
            auto stream = requestUrl.createInputStream(false, nullptr, nullptr, headersString,
                                                       30000, &responseHeaders, &statusCode);

            if (stream != nullptr)
            {
                juce::String responseString = stream->readEntireStreamAsString();
                DSTRACE << "DEBUG: OpenAI response status: " << statusCode << std::endl;

                if (statusCode == 200)
                {
                    // choices[0].message.content
                    juce::var parsed = juce::JSON::parse(responseString);
                    juce::String textContent = Surge::AI::JsonUtil::getStringAtPath(
                        parsed, {"choices", 0, "message", "content"});

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
                        response.errorMessage = "Failed to parse OpenAI response content";
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
                response.errorMessage = "Failed to connect to OpenAI API";
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

std::vector<Surge::AI::PatchModification>
APIClient::extractModifications(const std::string &responseText)
{
    return Surge::AI::JsonUtil::parseParameterList(responseText);
}
