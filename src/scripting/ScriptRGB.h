#pragma once
#include <string>

struct ScriptRGB {
    int r, g, b;

    ScriptRGB();
    ScriptRGB(int r, int g, int b);
    ScriptRGB(const ScriptRGB& other);

    // Operators
    ScriptRGB& operator=(const ScriptRGB& other);
    bool operator==(const ScriptRGB& other) const;
    bool operator!=(const ScriptRGB& other) const;

    // Methods
    ScriptRGB Lerp(const ScriptRGB& other, float t) const;
    void Clamp();
    std::string ToString() const;

    // Static constructors
    static ScriptRGB FromString(const std::string& str);
    static ScriptRGB Default();
    static ScriptRGB White();
    static ScriptRGB Black();
    static ScriptRGB Red();
    static ScriptRGB Green();
    static ScriptRGB Blue();
    static ScriptRGB Yellow();
    static ScriptRGB Cyan();
    static ScriptRGB Magenta();
};