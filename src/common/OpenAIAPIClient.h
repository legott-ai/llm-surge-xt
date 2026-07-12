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

#ifndef SURGE_SRC_COMMON_OPENAIAPICLIENT_H
#define SURGE_SRC_COMMON_OPENAIAPICLIENT_H

#include "AIModelProvider.h"
#include "PatchVectorDB.h"
#include <string>
#include <functional>
#include <memory>
#include <vector>

class SurgeStorage;

namespace Surge
{
namespace OpenAI
{

class APIClient : public Surge::AI::AIModelProvider
{
  public:
    explicit APIClient(SurgeStorage *storage,
                       Surge::AI::ModelType modelType = Surge::AI::ModelType::OPENAI_GPT5O);
    ~APIClient() override;

    // AIModelProvider interface
    Surge::AI::ModelType getModelType() const override { return currentModel; }
    std::string getDisplayName() const override { return Surge::AI::getModelName(currentModel); }
    bool isAPIKeyValid() const override;
    void setAPIKey(const std::string &key) override;
    std::string getAPIKey() const override;
    void setVectorDatabase(std::shared_ptr<Surge::PatchDB::VectorDatabase> db) override;

    void generatePatch(const std::string &prompt,
                       std::function<void(const Surge::AI::AIResponse &)> callback) override;

    void modifyPatch(const std::string &prompt, const std::string &currentPatchXML,
                     std::function<void(const Surge::AI::AIResponse &)> callback) override;

    // OpenAI-specific methods
    void setModel(Surge::AI::ModelType type) { currentModel = type; }

  private:
    void makeAPIRequest(const std::string &prompt, const std::string &context,
                        std::function<void(const Surge::AI::AIResponse &)> callback);

    std::vector<Surge::AI::PatchModification> extractModifications(const std::string &responseText);
    std::string generateEnhancedPrompt(const std::string &userPrompt);
    std::vector<std::string> extractSearchTerms(const std::string &prompt);
    std::string formatSimilarPatches(const std::vector<Surge::PatchDB::PatchVector> &patches);

    std::string getModelEndpoint() const;

    SurgeStorage *storage;
    std::string apiKey;
    Surge::AI::ModelType currentModel;
    std::shared_ptr<Surge::PatchDB::VectorDatabase> vectorDatabase;
};

} // namespace OpenAI
} // namespace Surge

#endif // SURGE_SRC_COMMON_OPENAIAPICLIENT_H
