#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "angelscript.h"
#include <scriptdictionary/scriptdictionary.h>

class Entity;
class Bsp;
class ScriptVec3;

// Wrapper class for exposing Entity to AngelScript
class ScriptEntity {
private:
    Entity* entity;
    Bsp* map;
    int entIdx;
    int refCount;

public:
    ScriptEntity();
    ScriptEntity(Entity* ent, Bsp* map, int entIdx);

	// Keyvalues
    CScriptDictionary* GetKeyValues() const;
    std::string GetKeyValue(const std::string& key) const;
    void SetKeyValue(const std::string& key, const std::string& value);
    bool HasKeyValue(const std::string& key) const;
    void RemoveKeyValue(const std::string& key);

	// Classname and targetname
    std::string GetClassname() const;
	void SetClassname(const std::string& classname);
    std::string GetTargetname() const;
	void SetTargetname(const std::string& targetname);

	// Position and orientation
    ScriptVec3 GetOrigin() const;
    void SetOrigin(const ScriptVec3& origin);
    ScriptVec3 GetAngles() const;
    void SetAngles(const ScriptVec3& angles);
    
	// Bsp model index (0 for worldspawn, -1 for non-BSP models)
    int GetBspModelIdx() const;
    bool IsBspModel() const;

    // Index in entity list
    int GetIndex() const;

    // Utility
    bool IsValid() const;
    float DistanceTo(const ScriptEntity& other) const;
	bool Intersects(const ScriptEntity& other) const;

    // Reference counting for AngelScript
    void AddRef();
    void Release();

    // Getter/Setter for internal Entity pointer (use with caution)
    Entity* GetEntity() const { return entity; }
    void SetEntity(Entity* ent) { entity = ent; }
    int GetEntIdx() const { return entIdx; }
    void SetEntIdx(int idx) { entIdx = idx; }
};