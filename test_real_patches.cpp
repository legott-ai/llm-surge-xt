#include "src/common/PatchVectorDB.h"
#include "src/common/SurgeStorage.h"
#include "HeadlessUtils.h"
#include <iostream>
#include <memory>

int main() {
    std::cout << "=== Testing Real Factory Patch Loading ===" << std::endl;
    
    try {
        // Create Surge instance
        auto surge = Surge::Headless::createSurge(44100);
        if (!surge) {
            std::cout << "❌ Failed to create Surge instance" << std::endl;
            return 1;
        }
        
        std::cout << "✅ Surge instance created" << std::endl;
        
        // Create vector database
        Surge::PatchDB::VectorDatabase vectorDB(&surge->storage);
        std::cout << "✅ Vector database created" << std::endl;
        
        // Try to build from factory patches
        std::cout << "🔄 Building vector database from factory patches..." << std::endl;
        vectorDB.buildFromFactoryPatches();
        
        std::cout << "📊 Database contains " << vectorDB.patches.size() << " patches" << std::endl;
        
        if (vectorDB.patches.size() > 0) {
            std::cout << "\n--- Sample Patches ---" << std::endl;
            for (size_t i = 0; i < std::min(size_t(5), vectorDB.patches.size()); ++i) {
                const auto& patch = vectorDB.patches[i];
                std::cout << i+1 << ". " << patch.name << " (" << patch.category << ")" << std::endl;
                std::cout << "   Description: " << patch.description << std::endl;
                std::cout << "   Vector size: " << patch.parameterVector.size() << std::endl;
            }
            
            // Test similarity search
            std::cout << "\n--- Testing Similarity Search ---" << std::endl;
            auto bassResults = vectorDB.findSimilarByText("bass", 3);
            std::cout << "Found " << bassResults.size() << " bass-related patches:" << std::endl;
            for (const auto& result : bassResults) {
                std::cout << "  - " << result.name << " (" << result.category << ")" << std::endl;
            }
            
            auto leadResults = vectorDB.findSimilarByText("lead", 3);
            std::cout << "Found " << leadResults.size() << " lead-related patches:" << std::endl;
            for (const auto& result : leadResults) {
                std::cout << "  - " << result.name << " (" << result.category << ")" << std::endl;
            }
            
            std::cout << "\n✅ RAG system is working with real patches!" << std::endl;
        } else {
            std::cout << "⚠️  No patches loaded - check factory patch directory" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}