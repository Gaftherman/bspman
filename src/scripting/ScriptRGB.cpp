#include "ScriptRGB.h"

ScriptRGB::ScriptRGB() : r(255), g(255), b(255) {}
ScriptRGB::ScriptRGB(int r_, int g_, int b_) : r(r_), g(g_), b(b_) {}
ScriptRGB::ScriptRGB(const ScriptRGB& other) : r(other.r), g(other.g), b(other.b) {}

ScriptRGB& ScriptRGB::operator=(const ScriptRGB& other) {
    r = other.r; g = other.g; b = other.b;
    return *this;
}

bool ScriptRGB::operator==(const ScriptRGB& other) const {
    return r == other.r && g == other.g && b == other.b;
}

bool ScriptRGB::operator!=(const ScriptRGB& other) const {
    return !(*this == other);
}

ScriptRGB ScriptRGB::Lerp(const ScriptRGB& other, float t) const {
    return ScriptRGB(
        (int)(r + (other.r - r) * t),
        (int)(g + (other.g - g) * t),
        (int)(b + (other.b - b) * t)
    );
}

void ScriptRGB::Clamp() {
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
}

std::string ScriptRGB::ToString() const {
    char buf[64];
    snprintf(buf, sizeof(buf), "%d %d %d", r, g, b);
    return std::string(buf);
}

ScriptRGB ScriptRGB::FromString(const std::string& str) {
    ScriptRGB c;
    if (sscanf(str.c_str(), "%d %d %d", &c.r, &c.g, &c.b))
    {
		return ScriptRGB::Default();
    }
    return c;
}

ScriptRGB ScriptRGB::Default() { return ScriptRGB(255, 255, 255); }
ScriptRGB ScriptRGB::White() { return ScriptRGB(255, 255, 255); }
ScriptRGB ScriptRGB::Black() { return ScriptRGB(0, 0, 0); }
ScriptRGB ScriptRGB::Red() { return ScriptRGB(255, 0, 0); }
ScriptRGB ScriptRGB::Green() { return ScriptRGB(0, 255, 0); }
ScriptRGB ScriptRGB::Blue() { return ScriptRGB(0, 0, 255); }
ScriptRGB ScriptRGB::Yellow() { return ScriptRGB(255, 255, 0); }
ScriptRGB ScriptRGB::Cyan() { return ScriptRGB(0, 255, 255); }
ScriptRGB ScriptRGB::Magenta() { return ScriptRGB(255, 0, 255); }
