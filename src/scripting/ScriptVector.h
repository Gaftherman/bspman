#pragma once
#include <string>
#include <vector>

class vec3;

struct ScriptVec3 {
    float x, y, z;

    ScriptVec3();
    ScriptVec3(float x, float y, float z);
    ScriptVec3(const ScriptVec3& other);
    ScriptVec3(const vec3& other);

    // Operators
    ScriptVec3& operator=(const ScriptVec3& other);
    ScriptVec3 operator+(const ScriptVec3& other) const;
    ScriptVec3 operator-(const ScriptVec3& other) const;
    ScriptVec3 operator*(float scalar) const;
    ScriptVec3 operator/(float scalar) const;
    ScriptVec3 operator-() const;
    bool operator==(const ScriptVec3& other) const;
    bool operator!=(const ScriptVec3& other) const;
    ScriptVec3& operator+=(const ScriptVec3& other);
    ScriptVec3& operator-=(const ScriptVec3& other);
    ScriptVec3& operator*=(float scalar);
    ScriptVec3& operator/=(float scalar);

    // Methods
    float Length() const;
    float Dot(const ScriptVec3& other) const;
    ScriptVec3 Cross(const ScriptVec3& other) const;
    void Normalize();
    float Distance(const ScriptVec3& other) const;
    ScriptVec3 Lerp(const ScriptVec3& other, float t) const;
    std::string ToString() const;

    // Convert to Entity Keyvalue format
    std::string ToKeyvalueString(bool truncate = false, std::string suffix_x = " ", std::string suffix_y = " ", std::string suffix_z = "") const;

    // Static constructors
    static ScriptVec3 FromString(const std::string& str);
    static ScriptVec3 Zero();
    static ScriptVec3 One();
    static ScriptVec3 Up();
    static ScriptVec3 Forward();
    static ScriptVec3 Right();
};