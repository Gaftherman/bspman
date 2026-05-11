#include <cmath>
#include <string>
#include "ScriptVector.h"
#include "vectors.h"

ScriptVec3::ScriptVec3() : x(0), y(0), z(0) {}
ScriptVec3::ScriptVec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
ScriptVec3::ScriptVec3(const ScriptVec3& other) : x(other.x), y(other.y), z(other.z) {}
ScriptVec3::ScriptVec3(const vec3& other) {
    this->x = other.x;
    this->y = other.y;
    this->z = other.z;
}

ScriptVec3& ScriptVec3::operator=(const ScriptVec3& other) {
    x = other.x; y = other.y; z = other.z;
    return *this;
}

ScriptVec3 ScriptVec3::operator+(const ScriptVec3& other) const {
    return ScriptVec3(x + other.x, y + other.y, z + other.z);
}

ScriptVec3 ScriptVec3::operator-(const ScriptVec3& other) const {
    return ScriptVec3(x - other.x, y - other.y, z - other.z);
}

ScriptVec3 ScriptVec3::operator*(float scalar) const {
    return ScriptVec3(x * scalar, y * scalar, z * scalar);
}

ScriptVec3 ScriptVec3::operator/(float scalar) const {
    if (scalar == 0) return ScriptVec3();
    return ScriptVec3(x / scalar, y / scalar, z / scalar);
}

ScriptVec3 ScriptVec3::operator-() const {
    return ScriptVec3(-x, -y, -z);
}

bool ScriptVec3::operator==(const ScriptVec3& other) const {
    return x == other.x && y == other.y && z == other.z;
}

bool ScriptVec3::operator!=(const ScriptVec3& other) const {
    return !(*this == other);
}

ScriptVec3& ScriptVec3::operator+=(const ScriptVec3& other) {
    x += other.x; y += other.y; z += other.z;
    return *this;
}

ScriptVec3& ScriptVec3::operator-=(const ScriptVec3& other) {
    x -= other.x; y -= other.y; z -= other.z;
    return *this;
}

ScriptVec3& ScriptVec3::operator*=(float scalar) {
    x *= scalar; y *= scalar; z *= scalar;
    return *this;
}

ScriptVec3& ScriptVec3::operator/=(float scalar) {
    if (scalar != 0) { x /= scalar; y /= scalar; z /= scalar; }
    return *this;
}

float ScriptVec3::Length() const {
    return std::sqrt(x * x + y * y + z * z);
}

float ScriptVec3::Dot(const ScriptVec3& other) const {
    return x * other.x + y * other.y + z * other.z;
}

ScriptVec3 ScriptVec3::Cross(const ScriptVec3& other) const {
    return ScriptVec3(
        y * other.z - z * other.y,
        z * other.x - x * other.z,
        x * other.y - y * other.x
    );
}

void ScriptVec3::Normalize() {
    float len = Length();
    if (len > 0) {
        x /= len; y /= len; z /= len;
    }
}

float ScriptVec3::Distance(const ScriptVec3& other) const {
    return (*this - other).Length();
}

ScriptVec3 ScriptVec3::Lerp(const ScriptVec3& other, float t) const {
    return ScriptVec3(
        x + (other.x - x) * t,
        y + (other.y - y) * t,
        z + (other.z - z) * t
    );
}

std::string ScriptVec3::ToString() const {
    char buf[128];
    snprintf(buf, sizeof(buf), "%g %g %g", x, y, z);
    return std::string(buf);
}

std::string ScriptVec3::ToKeyvalueString(bool truncate, std::string suffix_x, std::string suffix_y, std::string suffix_z) const {
    std::string parts[3] = { std::to_string(x) , std::to_string(y), std::to_string(z) };

    // remove trailing zeros to save some space
    for (int i = 0; i < 3; i++) {
        if (truncate) {
            size_t dotPos = parts[i].find(".");
            if (dotPos != std::string::npos) {
                parts[i] = parts[i].substr(0, dotPos + 3);
            }
        }

        size_t lastNotZero = parts[i].find_last_not_of('0');
        if (lastNotZero != std::string::npos) {
            parts[i].erase(lastNotZero + 1, std::string::npos);
        }

        // strip dot if there's no fractional part
        if (!parts[i].empty() && parts[i].back() == '.') {
            parts[i].pop_back();
        }
    }

    return parts[0] + suffix_x + parts[1] + suffix_y + parts[2] + suffix_z;
}

ScriptVec3 ScriptVec3::FromString(const std::string& str) {
    ScriptVec3 v;
    if( sscanf(str.c_str(), "%f %f %f", &v.x, &v.y, &v.z) != 3) {
        return ScriptVec3::Zero();
	}
    return v;
}

ScriptVec3 ScriptVec3::Zero() { return ScriptVec3(0, 0, 0); }
ScriptVec3 ScriptVec3::One() { return ScriptVec3(1, 1, 1); }
ScriptVec3 ScriptVec3::Up() { return ScriptVec3(0, 0, 1); }
ScriptVec3 ScriptVec3::Forward() { return ScriptVec3(1, 0, 0); }
ScriptVec3 ScriptVec3::Right() { return ScriptVec3(0, 1, 0); }