#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "angelscript.h"

class Entity;
class Bsp;
class Renderer;
class CScriptArray;
class ScriptEntity;
class ScriptVec3;

// Script information structure
struct ScriptInfo {
    std::string name;
    std::string path;
    bool hasError;
    std::string errorMessage;
};

// Script folder structure for hierarchical menu
struct ScriptFolder {
    std::string name;
    std::string path;
    std::vector<ScriptInfo> scripts;
    std::vector<ScriptFolder> subfolders;
};

class ScriptManager {
public:
    ScriptManager();
    ~ScriptManager();
    
    // Initialize AngelScript engine
    bool init(Renderer* renderer);
    bool initCLI(Bsp* map);
    void shutdown();
    
    // Script file management
    void refreshScriptList();
    std::vector<ScriptInfo>& getScriptList();
    ScriptFolder& getScriptRoot();  // Hierarchical folder structure
    std::string getScriptsFolder() const;
    void openScriptsFolder();
    
    // Script execution
    bool loadScript(const std::string& scriptPath);
    bool executeScript(const std::string& scriptPath);
    bool executeFunction(const std::string& functionName);
    
    // Entity access for scripts
    ScriptEntity* getEntity(int index);
    int getEntityCount();
    ScriptEntity* getEntityByTargetname(const std::string& targetname);
    ScriptEntity* getEntityByClassname(const std::string& classname);
    ScriptEntity* createEntity(const std::string& classname);
    void deleteEntity(int index);
    
    // Visual refresh after modifications
    void refreshEntityDisplay();
    
    // Get all entities by classname (returns array)
    std::vector<ScriptEntity*> getAllEntitiesByClassname(const std::string& classname);
    
    // Utility functions for scripts
    void print(const std::string& message);
    void printWarning(const std::string& message);
    void printError(const std::string& message);
    
    // Map access
    std::string getMapName() const;
    
    // Camera access
    float getCameraX() const;
    float getCameraY() const;
    float getCameraZ() const;
    float getCameraAnglesPitch() const;
    float getCameraAnglesYaw() const;
    float getCameraAnglesRoll() const;
    ScriptVec3 getCameraPosition() const;
    ScriptVec3 getCameraAnglesVec() const;
    ScriptVec3 getCameraForward() const;  // Direction the camera is looking
    
    // Entity selection (highlight)
    CScriptArray* getSelectedEntities();  // Returns array of selected Entity@
    int getSelectedEntityCount() const;
    void selectEntity(int index);
    void deselectEntity(int index);
    void deselectAllEntities();
    bool isEntitySelected(int index) const;
    
    // Script entity batch operations (for undo grouping)
    void beginEntityBatch();
    void endEntityBatch();
    
    // Math utility functions
    static float degToRad(float degrees);
    static float radToDeg(float radians);

    // Angelscript predefined generator
    void generateScriptPredefined(const std::string& path);

private:
    asIScriptEngine* engine;
    asIScriptContext* context;
    asIScriptModule* currentModule;
    Renderer* app;
    
    std::vector<ScriptInfo> scriptList;
    ScriptFolder scriptRoot;  // Hierarchical folder structure
    std::string scriptsFolder;

    // For CLI mode
	Bsp* cliMap;
    Bsp* getCurrentMap() const;
    
    void scanScriptsFolder(const std::string& folderPath, ScriptFolder& folder);
    
    // Entity wrappers cache
    std::unordered_map<int, ScriptEntity*> entityCache;
    
    // Entity batch for undo grouping
    std::vector<Entity*> batchCreatedEntities;
    bool isBatchMode;
    
    void clearEntityCache();
    
    // Registration methods
    void registerTypes();
    void registerEntityMethods();
    void registerGlobalFunctions();
    void registerMathTypes();
    void registerArrayExtensions();
    
    // AngelScript callbacks
    static void messageCallback(const asSMessageInfo* msg, void* param);
};

// Global script manager instance
extern ScriptManager* g_scriptManager;
