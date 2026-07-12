#include "src/common/PatchVectorDB.h"
#include "src/common/SurgeStorage.h"
#include <iostream>
#include <memory>

int main() {
    std::cout << "Testing Patch Vector Database..." << std::endl;
    
    // Create a minimal SurgeStorage instance
    auto storage = std::make_unique<SurgeStorage>("./resources/data");
    
    // Create vector database
    Surge::PatchDB::VectorDatabase vectorDB(storage.get());
    
    // Try to build from factory patches
    std::cout << "Building vector database from factory patches..." << std::endl;
    vectorDB.buildFromFactoryPatches();
    
    // Test similarity search with a simple text query
    std::cout << "Testing text-based similarity search..." << std::endl;
    auto results = vectorDB.findSimilarByText("bass", 5);
    
    std::cout << "Found " << results.size() << " similar patches for 'bass':" << std::endl;
    for (const auto& result : results) {
        std::cout << "  - " << result.name << " (" << result.category << ")" << std::endl;
        std::cout << "    Description: " << result.description << std::endl;
    }
    
    return 0;
}