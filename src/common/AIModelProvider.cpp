// Simple factory implementation for AIModelProvider
// Currently only OpenAI is supported

#include "AIModelProvider.h"
#include "OpenAIAPIClient.h"
#include "ClaudeAPIClient.h"

namespace Surge
{
namespace AI
{

// Factory function implementation
std::unique_ptr<AIModelProvider> createProvider(ModelType type, SurgeStorage *storage)
{
    if (isClaudeModel(type))
    {
        return std::make_unique<Surge::Claude::APIClient>(storage, type);
    }

    // Default to OpenAI for GPT models or fallback
    return std::make_unique<Surge::OpenAI::APIClient>(storage, type);
}

} // namespace AI
} // namespace Surge
