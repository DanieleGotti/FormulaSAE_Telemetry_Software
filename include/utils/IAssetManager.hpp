#include <string>
#include <iostream>
#include <vector>
#include <fstream>

class IAssetManager {
public:
    virtual ~IAssetManager() = default;

    virtual void* loadFont(const std::string& name);
    virtual void* loadImage(const std::string& name);
};