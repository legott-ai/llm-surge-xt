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

#ifndef SURGE_SRC_COMMON_AIMODELPROVIDER_H
#define SURGE_SRC_COMMON_AIMODELPROVIDER_H

#include <string>
#include <vector>
#include <functional>
#include <memory>

class SurgeStorage;

namespace Surge
{
namespace PatchDB
{
class VectorDatabase;
}

namespace AI
{

// Unified modification structure for all AI providers
struct PatchModification
{
    std::string parameterName;
    float value;
    std::string description;
};

// Unified response structure for all AI providers
struct AIResponse
{
    bool success{false};
    std::string responseText;
    std::string errorMessage;
    std::vector<PatchModification> modifications;
};

// Available AI model types. The enum order is also the order the DeepSynth
// panel lists them in its dropdown (combo id == enum index + 1), so keep the
// three families grouped and contiguous. NUM_MODELS is a sentinel used for the
// dropdown/state bounds — leave it last.
enum class ModelType
{
    // Google Gemini
    GEMINI_3_PRO,
    GEMINI_3_FLASH,
    GEMINI_2_5_FLASH,
    // Anthropic Claude
    CLAUDE_OPUS_4_8,
    CLAUDE_SONNET_5,
    CLAUDE_HAIKU_4_5,
    // OpenAI
    OPENAI_GPT5,
    OPENAI_GPT5O,

    NUM_MODELS
};

// Get human-readable name for model type
inline std::string getModelName(ModelType type)
{
    switch (type)
    {
    case ModelType::GEMINI_3_PRO:
        return "Gemini 3 Pro";
    case ModelType::GEMINI_3_FLASH:
        return "Gemini 3 Flash";
    case ModelType::GEMINI_2_5_FLASH:
        return "Gemini 2.5 Flash";
    case ModelType::CLAUDE_OPUS_4_8:
        return "Claude Opus 4.8";
    case ModelType::CLAUDE_SONNET_5:
        return "Claude Sonnet 5";
    case ModelType::CLAUDE_HAIKU_4_5:
        return "Claude Haiku 4.5";
    case ModelType::OPENAI_GPT5:
        return "GPT-5";
    case ModelType::OPENAI_GPT5O:
        return "GPT-5o";
    default:
        return "Unknown";
    }
}

inline bool isGeminiModel(ModelType type)
{
    return type == ModelType::GEMINI_3_PRO || type == ModelType::GEMINI_3_FLASH ||
           type == ModelType::GEMINI_2_5_FLASH;
}

inline bool isClaudeModel(ModelType type)
{
    return type == ModelType::CLAUDE_OPUS_4_8 || type == ModelType::CLAUDE_SONNET_5 ||
           type == ModelType::CLAUDE_HAIKU_4_5;
}

// Abstract interface for AI model providers
class AIModelProvider
{
  public:
    virtual ~AIModelProvider() = default;

    // Get the model type
    virtual ModelType getModelType() const = 0;

    // Get the display name
    virtual std::string getDisplayName() const = 0;

    // Check if API key is configured
    virtual bool isAPIKeyValid() const = 0;

    // Set API key
    virtual void setAPIKey(const std::string &key) = 0;

    // Get API key
    virtual std::string getAPIKey() const = 0;

    // Set vector database for RAG
    virtual void setVectorDatabase(std::shared_ptr<Surge::PatchDB::VectorDatabase> db) = 0;

    // Generate a new patch from prompt
    virtual void generatePatch(const std::string &prompt,
                               std::function<void(const AIResponse &)> callback) = 0;

    // Modify existing patch
    virtual void modifyPatch(const std::string &prompt, const std::string &currentPatchXML,
                             std::function<void(const AIResponse &)> callback) = 0;
};

// Factory function to create AI provider by type
std::unique_ptr<AIModelProvider> createProvider(ModelType type, SurgeStorage *storage);

} // namespace AI
} // namespace Surge

#endif // SURGE_SRC_COMMON_AIMODELPROVIDER_H
