#include <iostream>
#include "SurgeSynthesizer.h"
#include "SurgeStorage.h"

int main()
{
    auto surge = std::make_unique<SurgeSynthesizer>(nullptr, "");
    
    std::cout << "Total parameters: " << surge->storage.getPatch().param_ptr.size() << std::endl;
    
    // Print parameters that contain "Osc" and "Type" in their names
    std::cout << "\nSearching for oscillator type parameters:\n" << std::endl;
    
    for (int i = 0; i < surge->storage.getPatch().param_ptr.size(); i++)
    {
        auto param = surge->storage.getPatch().param_ptr[i];
        if (param)
        {
            char paramName[TXT_SIZE];
            surge->getParameterName(i, paramName);
            std::string fullName(paramName);
            
            // Look for parameters with "Osc" and "Type" in the name
            if (fullName.find("Osc") != std::string::npos && 
                fullName.find("Type") != std::string::npos)
            {
                std::cout << "Parameter[" << i << "]: " << fullName 
                          << " (scene=" << param->scene << ")" << std::endl;
            }
        }
    }
    
    return 0;
}