#include "ScriptEntity.h"
#include "ScriptVector.h"
#include "Entity.h"

ScriptEntity::ScriptEntity() : entity(nullptr), map(nullptr), entIdx(-1), refCount(1) {}

ScriptEntity::ScriptEntity(Entity* ent, Bsp* map, int entIdx) {
	this->entity = ent;
	this->map = map;
	this->entIdx = entIdx;
    refCount = 1;
}

void ScriptEntity::AddRef() {
    refCount++;
}

void ScriptEntity::Release() {
    if (--refCount == 0) {
        delete this;
    }
}

CScriptDictionary* ScriptEntity::GetKeyValues() const {

    asIScriptContext* ctx = asGetActiveContext();

	if (!ctx) // Should never happen, but just in case
        return nullptr;

    asIScriptEngine* engine = ctx->GetEngine();
    CScriptDictionary* dict = CScriptDictionary::Create(engine);
    auto keyvalues = entity->getAllKeyvalues();

    for (const auto& pair : keyvalues)
    {
        int stringTypeId = engine->GetTypeIdByDecl("string");
        dict->Set(pair.first, (void*)&pair.second, stringTypeId);
    }

	return dict;
}

std::string ScriptEntity::GetKeyValue(const std::string& key) const {
    return entity->getKeyvalue(key);
}

void ScriptEntity::SetKeyValue(const std::string& key, const std::string& value) {
    entity->setOrAddKeyvalue(key, value);
}

bool ScriptEntity::HasKeyValue(const std::string& key) const {
    return entity->hasKey(key);
}

void ScriptEntity::RemoveKeyValue(const std::string& key) {
    entity->removeKeyvalue(key);
}

std::string ScriptEntity::GetClassname() const {
    return entity->getClassname();
}

void ScriptEntity::SetClassname(const std::string& classname) {
    entity->setOrAddKeyvalue("classname", classname);
}

std::string ScriptEntity::GetTargetname() const {
    return entity->getTargetname();
}

void ScriptEntity::SetTargetname(const std::string& targetname) {
    entity->setOrAddKeyvalue("targetname", targetname);
}

ScriptVec3 ScriptEntity::GetOrigin() const {
    return ScriptVec3(entity->getOrigin());
}

void ScriptEntity::SetOrigin(const ScriptVec3& origin) {
    entity->setOrAddKeyvalue("origin", origin.ToKeyvalueString());
}

ScriptVec3 ScriptEntity::GetAngles() const {
    return ScriptVec3(entity->getAngles());
}

void ScriptEntity::SetAngles(const ScriptVec3& angles) {
    entity->setOrAddKeyvalue("angles", angles.ToKeyvalueString());
}

int ScriptEntity::GetBspModelIdx() const {
    return entity->getBspModelIdx();
}

bool ScriptEntity::IsBspModel() const {
    return entity->isBspModel();
}

int ScriptEntity::GetIndex() const {
    return entIdx;
}

bool ScriptEntity::IsValid() const {
    return entity != nullptr;
}

float ScriptEntity::DistanceTo(const ScriptEntity& other) const {
    if (!other.IsValid()) 
        return -1.0f;

    return (entity->getOrigin() - other.entity->getOrigin()).length();
}

bool ScriptEntity::Intersects(const ScriptEntity& other) const {
    if (!other.IsValid()) 
        return false;

    vec3 myMin = entity->drawOrigin - entity->drawMin;
    vec3 myMax = entity->drawOrigin + entity->drawMax;
    vec3 otherMin = other.entity->drawOrigin - other.entity->drawMin;
    vec3 otherMax = other.entity->drawOrigin + other.entity->drawMax;
    return (myMin.x <= otherMax.x && myMax.x >= otherMin.x) &&
           (myMin.y <= otherMax.y && myMax.y >= otherMin.y) &&
           (myMin.z <= otherMax.z && myMax.z >= otherMin.z);
}