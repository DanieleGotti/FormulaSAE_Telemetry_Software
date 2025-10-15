#pragma once

class UIElement {
public:
    virtual ~UIElement() = default;
    
    virtual void draw() = 0;
};