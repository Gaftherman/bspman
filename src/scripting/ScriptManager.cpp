#include "ScriptManager.h"
#include "Renderer.h"
#include "BspRenderer.h"
#include "Gui.h"
#include "Bsp.h"
#include "Entity.h"
#include "Command.h"
#include "util.h"
#include "vectors.h"
#include "globals.h"

#include "angelscript.h"
#include "scriptbuilder/scriptbuilder.h"
#include "scriptstdstring/scriptstdstring.h"
#include "scriptarray/scriptarray.h"
#include "scriptmath/scriptmath.h"
#include "scripthelper/scripthelper.h"
#include "scriptfile/scriptfile.h"
#include "scriptfile/scriptfilesystem.h"
#include "datetime/datetime.h"
#include <scriptdictionary/scriptdictionary.h>

#include <fstream>
#include <sstream>
#include <cassert>
#include <algorithm>
#include <cmath>
#include <functional>

#include "ScriptRGB.h"
#include "ScriptEntity.h"
#include "ScriptVector.h"

#ifdef _WIN32
#include <Windows.h>
#include <ShlObj.h>
// Undefine Windows macros that conflict with std::min/std::max
#undef min
#undef max
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Global script manager instance
ScriptManager* g_scriptManager = nullptr;

// ============================================================================
// ScriptManager Implementation
// ============================================================================

ScriptManager::ScriptManager() 
    : engine(nullptr), context(nullptr), currentModule(nullptr), app(nullptr), isBatchMode(false) {
    
    // Get Documents folder path
#ifdef _WIN32
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &path))) {
        std::wstring wpath(path);
        scriptsFolder = std::string(wpath.begin(), wpath.end()) + "\\bspguy\\scripts";
        CoTaskMemFree(path);
    } else {
        // Fallback: use USERPROFILE environment variable
        const char* userProfile = getenv("USERPROFILE");
        if (userProfile) {
            scriptsFolder = std::string(userProfile) + "\\Documents\\bspguy\\scripts";
        } else {
            // Last resort: use current directory
            scriptsFolder = ".\\scripts";
        }
    }
#else
    const char* home = getenv("HOME");
    if (home) {
        scriptsFolder = std::string(home) + "/Documents/bspguy/scripts";
    } else {
        scriptsFolder = "./scripts";
    }
#endif
}

ScriptManager::~ScriptManager() {
    shutdown();
}

void ScriptManager::messageCallback(const asSMessageInfo* msg, void* param) {
    const char* type = "ERR ";
    if (msg->type == asMSGTYPE_WARNING)
        type = "WARN";
    else if (msg->type == asMSGTYPE_INFORMATION)
        type = "INFO";
    
    logf("[AngelScript %s] %s (%d, %d): %s\n", type, msg->section, msg->row, msg->col, msg->message);
}

bool ScriptManager::init(Renderer* renderer) {
    app = renderer;
    
    // Create the script engine
    engine = asCreateScriptEngine();
    if (!engine) {
        logf("Failed to create AngelScript engine\n");
        return false;
    }
    
    // Set the message callback
    engine->SetMessageCallback(asFUNCTION(messageCallback), nullptr, asCALL_CDECL);
    
    // Register standard add-ons
    RegisterStdString(engine);
    RegisterScriptArray(engine, true);
    registerArrayExtensions();
    RegisterStdStringUtils(engine);
    RegisterScriptDictionary(engine);
    RegisterScriptDateTime(engine);
    RegisterScriptMath(engine);
	RegisterScriptFile(engine);
	RegisterScriptFileSystem(engine);
    RegisterExceptionRoutines(engine);

    // Register custom types and functions
    registerMathTypes();
    registerTypes();
    registerEntityMethods();
    registerGlobalFunctions();
    
    // Create a context for execution
    context = engine->CreateContext();
    if (!context) {
        logf("Failed to create AngelScript context\n");
        return false;
    }
    
    // Create scripts folder if it doesn't exist
    if (!dirExists(scriptsFolder)) {
        createDir(scriptsFolder);
        logf("Created scripts folder: %s\n", scriptsFolder.c_str());
    }
    
    refreshScriptList();
    
    logf("AngelScript initialized. Scripts folder: %s\n", scriptsFolder.c_str());
    return true;
}

bool ScriptManager::initCLI(Bsp* map) {
    app = nullptr;
    cliMap = map;
    isBatchMode = false;

    engine = asCreateScriptEngine();
    if (!engine) {
        logf("Failed to create AngelScript engine\n");
        return false;
    }

    engine->SetMessageCallback(asFUNCTION(messageCallback), nullptr, asCALL_CDECL);

    RegisterStdString(engine);
    RegisterScriptArray(engine, true);
    registerArrayExtensions();
    RegisterStdStringUtils(engine);
    RegisterScriptDictionary(engine);
    RegisterScriptDateTime(engine);
    RegisterScriptMath(engine);
    RegisterScriptFile(engine);
    RegisterScriptFileSystem(engine);
    RegisterExceptionRoutines(engine);

    registerMathTypes();
    registerTypes();
    registerEntityMethods();
    registerGlobalFunctions();

    context = engine->CreateContext();
    if (!context) {
        logf("Failed to create AngelScript context\n");
        return false;
    }

    return true;
}

Bsp* ScriptManager::getCurrentMap() const {
    if (cliMap) return cliMap;
    if (app && app->mapRenderer) return app->mapRenderer->map;
    return nullptr;
}

void ScriptManager::shutdown() {
    clearEntityCache();
    
    if (context) {
        context->Release();
        context = nullptr;
    }
    
    if (engine) {
        engine->ShutDownAndRelease();
        engine = nullptr;
    }
    
    currentModule = nullptr;
}

// Wrapper functions for global functions (AngelScript requires specific calling convention)
static void Script_print(const std::string& msg) {
    if (g_scriptManager) g_scriptManager->print(msg);
}

static void Script_printWarning(const std::string& msg) {
    if (g_scriptManager) g_scriptManager->printWarning(msg);
}

static void Script_printError(const std::string& msg) {
    if (g_scriptManager) g_scriptManager->printError(msg);
}

static int Script_getEntityCount() {
    if (g_scriptManager) return g_scriptManager->getEntityCount();
    return 0;
}

static ScriptEntity* Script_getEntity(int index) {
    if (g_scriptManager) return g_scriptManager->getEntity(index);
    return nullptr;
}

static ScriptEntity* Script_getEntityByTargetname(const std::string& name) {
    if (g_scriptManager) return g_scriptManager->getEntityByTargetname(name);
    return nullptr;
}

static ScriptEntity* Script_getEntityByClassname(const std::string& name) {
    if (g_scriptManager) return g_scriptManager->getEntityByClassname(name);
    return nullptr;
}

static ScriptEntity* Script_createEntity(const std::string& classname) {
    if (g_scriptManager) return g_scriptManager->createEntity(classname);
    return nullptr;
}

static void Script_deleteEntity(int index) {
    if (g_scriptManager) g_scriptManager->deleteEntity(index);
}

static std::string Script_getMapName() {
    if (g_scriptManager) return g_scriptManager->getMapName();
    return "";
}

static void Script_refreshEntities() {
    if (g_scriptManager) g_scriptManager->refreshEntityDisplay();
}

// Camera wrapper functions
static float Script_getCameraX() {
    if (g_scriptManager) return g_scriptManager->getCameraX();
    return 0.0f;
}

static float Script_getCameraY() {
    if (g_scriptManager) return g_scriptManager->getCameraY();
    return 0.0f;
}

static float Script_getCameraZ() {
    if (g_scriptManager) return g_scriptManager->getCameraZ();
    return 0.0f;
}

static float Script_getCameraPitch() {
    if (g_scriptManager) return g_scriptManager->getCameraAnglesPitch();
    return 0.0f;
}

static float Script_getCameraYaw() {
    if (g_scriptManager) return g_scriptManager->getCameraAnglesYaw();
    return 0.0f;
}

static float Script_getCameraRoll() {
    if (g_scriptManager) return g_scriptManager->getCameraAnglesRoll();
    return 0.0f;
}

static ScriptVec3 Script_getCameraPos() {
    if (g_scriptManager) return g_scriptManager->getCameraPosition();
    return ScriptVec3();
}

static ScriptVec3 Script_getCameraAngles() {
    if (g_scriptManager) return g_scriptManager->getCameraAnglesVec();
    return ScriptVec3();
}

static ScriptVec3 Script_getCameraForward() {
    if (g_scriptManager) return g_scriptManager->getCameraForward();
    return ScriptVec3(0, 1, 0);  // Default forward in bspguy is +Y
}

// Batch mode for grouping entity creation into single undo
static void Script_beginEntityBatch() {
    if (g_scriptManager) g_scriptManager->beginEntityBatch();
}

static void Script_endEntityBatch() {
    if (g_scriptManager) g_scriptManager->endEntityBatch();
}

// Entity selection functions
static CScriptArray* Script_getSelectedEntities() {
    if (g_scriptManager) return g_scriptManager->getSelectedEntities();
    return nullptr;
}

static int Script_getSelectedEntityCount() {
    if (g_scriptManager) return g_scriptManager->getSelectedEntityCount();
    return 0;
}

static void Script_selectEntity(int index) {
    if (g_scriptManager) g_scriptManager->selectEntity(index);
}

static void Script_deselectEntity(int index) {
    if (g_scriptManager) g_scriptManager->deselectEntity(index);
}

static void Script_deselectAllEntities() {
    if (g_scriptManager) g_scriptManager->deselectAllEntities();
}

static bool Script_isEntitySelected(int index) {
    if (g_scriptManager) return g_scriptManager->isEntitySelected(index);
    return false;
}

static float Script_degToRad(float degrees) {
    return ScriptManager::degToRad(degrees);
}

static float Script_radToDeg(float radians) {
    return ScriptManager::radToDeg(radians);
}

static float Script_round(float x) {
    return std::round(x);
}

static float Script_min(float a, float b) {
    return std::min(a, b);
}

static float Script_max(float a, float b) {
    return std::max(a, b);
}

static float Script_clamp(float value, float minVal, float maxVal) {
    return std::max(minVal, std::min(value, maxVal));
}

static float Script_lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static std::string Script_intToString(int value) {
    return std::to_string(value);
}

static std::string Script_floatToString(float value) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%g", value);
    return std::string(buf);
}

static int Script_stringToInt(const std::string& str) {
    return std::atoi(str.c_str());
}

static float Script_stringToFloat(const std::string& str) {
    return (float)std::atof(str.c_str());
}

void ScriptManager::refreshScriptList() {
    scriptList.clear();
    scriptRoot = ScriptFolder();
    scriptRoot.name = "Scripts";
    scriptRoot.path = scriptsFolder;
    
    if (!dirExists(scriptsFolder)) {
        createDir(scriptsFolder);
        return;
    }
    
    // Scan folder recursively
    scanScriptsFolder(scriptsFolder, scriptRoot);
    
    // Also populate flat list for backward compatibility
    std::function<void(const ScriptFolder&)> flattenScripts = [&](const ScriptFolder& folder) {
        for (const auto& script : folder.scripts) {
            scriptList.push_back(script);
        }
        for (const auto& subfolder : folder.subfolders) {
            flattenScripts(subfolder);
        }
    };
    flattenScripts(scriptRoot);
}

void ScriptManager::scanScriptsFolder(const std::string& folderPath, ScriptFolder& folder) {
#ifdef _WIN32
    // First scan for .as files
    std::string searchPath = folderPath + "\\*.as";
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                ScriptInfo info;
                info.name = findData.cFileName;
                info.path = folderPath + "\\" + findData.cFileName;
                info.hasError = false;
                folder.scripts.push_back(info);
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }
    
    // Sort scripts alphabetically
    std::sort(folder.scripts.begin(), folder.scripts.end(), 
        [](const ScriptInfo& a, const ScriptInfo& b) { return a.name < b.name; });
    
    // Then scan for subdirectories
    searchPath = folderPath + "\\*";
    hFind = FindFirstFileA(searchPath.c_str(), &findData);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                strcmp(findData.cFileName, ".") != 0 &&
                strcmp(findData.cFileName, "..") != 0) {
                
                ScriptFolder subfolder;
                subfolder.name = findData.cFileName;
                subfolder.path = folderPath + "\\" + findData.cFileName;
                scanScriptsFolder(subfolder.path, subfolder);
                
                // Only add non-empty folders
                if (!subfolder.scripts.empty() || !subfolder.subfolders.empty()) {
                    folder.subfolders.push_back(subfolder);
                }
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }
    
    // Sort subfolders alphabetically
    std::sort(folder.subfolders.begin(), folder.subfolders.end(), 
        [](const ScriptFolder& a, const ScriptFolder& b) { return a.name < b.name; });
#else
    // POSIX implementation
    DIR* dir = opendir(folderPath.c_str());
    if (dir) {
        struct dirent* entry;
        std::vector<std::string> subdirs;
        
        while ((entry = readdir(dir)) != nullptr) {
            std::string filename = entry->d_name;
            std::string fullPath = folderPath + "/" + filename;
            
            struct stat statbuf;
            if (stat(fullPath.c_str(), &statbuf) == 0) {
                if (S_ISDIR(statbuf.st_mode)) {
                    if (filename != "." && filename != "..") {
                        subdirs.push_back(filename);
                    }
                } else if (filename.size() > 3 && filename.substr(filename.size() - 3) == ".as") {
                    ScriptInfo info;
                    info.name = filename;
                    info.path = fullPath;
                    info.hasError = false;
                    folder.scripts.push_back(info);
                }
            }
        }
        closedir(dir);
        
        // Sort scripts
        std::sort(folder.scripts.begin(), folder.scripts.end(), 
            [](const ScriptInfo& a, const ScriptInfo& b) { return a.name < b.name; });
        
        // Process subdirectories
        std::sort(subdirs.begin(), subdirs.end());
        for (const auto& subdir : subdirs) {
            ScriptFolder subfolder;
            subfolder.name = subdir;
            subfolder.path = folderPath + "/" + subdir;
            scanScriptsFolder(subfolder.path, subfolder);
            
            if (!subfolder.scripts.empty() || !subfolder.subfolders.empty()) {
                folder.subfolders.push_back(subfolder);
            }
        }
    }
#endif
}

ScriptFolder& ScriptManager::getScriptRoot() {
    return scriptRoot;
}

std::vector<ScriptInfo>& ScriptManager::getScriptList() {
    return scriptList;
}

std::string ScriptManager::getScriptsFolder() const {
    return scriptsFolder;
}

void ScriptManager::openScriptsFolder() {
    if (!dirExists(scriptsFolder)) {
        createDir(scriptsFolder);
    }
    
#ifdef _WIN32
    ShellExecuteA(NULL, "explore", scriptsFolder.c_str(), NULL, NULL, SW_SHOWNORMAL);
#elif __APPLE__
    std::string cmd = "open \"" + scriptsFolder + "\"";
    system(cmd.c_str());
#else
    std::string cmd = "xdg-open \"" + scriptsFolder + "\"";
    system(cmd.c_str());
#endif
}

bool ScriptManager::loadScript(const std::string& scriptPath) {
    if (!engine) {
        logf("AngelScript engine not initialized\n");
        return false;
    }
    
    // Clear entity cache before running a new script
    clearEntityCache();
    
    // Create a new module for this script
    CScriptBuilder builder;
    int r = builder.StartNewModule(engine, "ScriptModule");
    if (r < 0) {
        logf("Failed to start new script module\n");
        return false;
    }
    
    r = builder.AddSectionFromFile(scriptPath.c_str());
    if (r < 0) {
        logf("Failed to load script file: %s\n", scriptPath.c_str());
        return false;
    }
    
    r = builder.BuildModule();
    if (r < 0) {
        logf("Failed to build script: %s\n", scriptPath.c_str());
        return false;
    }
    
    currentModule = engine->GetModule("ScriptModule");
    return true;
}

bool ScriptManager::executeScript(const std::string& scriptPath) {
    logf("\n--- Executing script: %s ---\n", basename(scriptPath).c_str());
    
    if (!loadScript(scriptPath)) {
        return false;
    }
    
    // Look for a main() function
    asIScriptFunction* func = currentModule->GetFunctionByDecl("void main()");
    if (!func) {
        logf("No main() function found in script. Looking for other entry points...\n");
        
        // Try other common entry point names
        func = currentModule->GetFunctionByDecl("void run()");
        if (!func) {
            func = currentModule->GetFunctionByDecl("void execute()");
        }
        if (!func) {
            logf("No entry point function found (main, run, or execute)\n");
            return false;
        }
    }
    
    // Execute the function
    context->Prepare(func);
    int r = context->Execute();
    
    if (r != asEXECUTION_FINISHED) {
        if (r == asEXECUTION_EXCEPTION) {
            logf("Script exception: %s\n", context->GetExceptionString());
            int line = context->GetExceptionLineNumber();
            logf("  at line %d\n", line);
            return false;
        }
    }
    
    logf("--- Script finished ---\n\n");
    return true;
}

bool ScriptManager::executeFunction(const std::string& functionName) {
    if (!currentModule || !context) {
        logf("No script loaded\n");
        return false;
    }
    
    std::string decl = "void " + functionName + "()";
    asIScriptFunction* func = currentModule->GetFunctionByDecl(decl.c_str());
    if (!func) {
        logf("Function not found: %s\n", functionName.c_str());
        return false;
    }
    
    context->Prepare(func);
    int r = context->Execute();
    
    if (r != asEXECUTION_FINISHED) {
        if (r == asEXECUTION_EXCEPTION) {
            logf("Script exception: %s\n", context->GetExceptionString());
            return false;
        }
    }
    
    return true;
}

void ScriptManager::clearEntityCache() {
    for (auto& pair : entityCache) {
        // Don't delete, let AngelScript handle it through reference counting
        if (pair.second) {
            pair.second->SetEntity(nullptr); // Invalidate the pointer
        }
    }
    entityCache.clear();
}

ScriptEntity* ScriptManager::getEntity(int index) {
    Bsp* map = getCurrentMap();
    if (!map) return nullptr;

    if (index < 0 || index >= (int)map->ents.size()) return nullptr;
    
    // Check cache
    auto it = entityCache.find(index);
    if (it != entityCache.end() && it->second->GetEntity()) {
        it->second->AddRef(); // Caller gets a reference
        return it->second;
    }
    
    // Create new wrapper
    ScriptEntity* wrapper = new ScriptEntity(map->ents[index], map, index);
    entityCache[index] = wrapper;
    wrapper->AddRef(); // Cache holds one reference
    return wrapper; // Return with refCount = 1 for the caller
}

int ScriptManager::getEntityCount() {
    Bsp* map = getCurrentMap();
    if (!map) return 0;

    return (int)map->ents.size();
}

ScriptEntity* ScriptManager::getEntityByTargetname(const std::string& targetname) {
    Bsp* map = getCurrentMap();
    if (!map) return nullptr;
    
    for (int i = 0; i < (int)map->ents.size(); i++) {
        if (map->ents[i]->getTargetname() == targetname) {
            return getEntity(i);
        }
    }
    return nullptr;
}

ScriptEntity* ScriptManager::getEntityByClassname(const std::string& classname) {
    Bsp* map = getCurrentMap();
    if (!map) return nullptr;
    
    for (int i = 0; i < (int)map->ents.size(); i++) {
        if (map->ents[i]->getClassname() == classname) {
            return getEntity(i);
        }
    }
    return nullptr;
}

ScriptEntity* ScriptManager::createEntity(const std::string& classname) {
    Bsp* map = getCurrentMap();
    if (!map) return nullptr;
    
    Entity* ent = new Entity(classname);
    
    // In batch mode, track entity for grouped undo but don't add to map yet
    // Actually we need to add to map immediately for the script to modify it
    // We'll track it for the undo command
    if (isBatchMode) {
        // Store a copy for the undo command
        batchCreatedEntities.push_back(ent);
    }
    
    map->ents.push_back(ent);
    
    int index = (int)map->ents.size() - 1;
    ScriptEntity* wrapper = new ScriptEntity(ent, map, index);
    entityCache[index] = wrapper;
    wrapper->AddRef();
    
    logf("Created entity: %s (index %d)%s\n", classname.c_str(), index, 
         isBatchMode ? " [batched]" : "");
    return wrapper;
}

void ScriptManager::deleteEntity(int index) {
    Bsp* map = getCurrentMap();
    if (!map) return;

    if (index <= 0 || index >= (int)map->ents.size()) {
        logf("Cannot delete entity at index %d (invalid or worldspawn)\n", index);
        return;
    }
    
    // Invalidate cache entry
    auto it = entityCache.find(index);
    if (it != entityCache.end()) {
        it->second->SetEntity(nullptr);
        entityCache.erase(it);
    }
    
    // Delete the entity
    delete map->ents[index];
    map->ents.erase(map->ents.begin() + index);
    
    // Update cache indices
    std::unordered_map<int, ScriptEntity*> newCache;
    for (auto& pair : entityCache) {
        if (pair.first > index) {
            pair.second->SetEntIdx(pair.first - 1);
            newCache[pair.first - 1] = pair.second;
        } else {
            newCache[pair.first] = pair.second;
        }
    }
    entityCache = newCache;
    
    logf("Deleted entity at index %d\n", index);
}

void ScriptManager::print(const std::string& message) {
    logf("[Script] %s\n", message.c_str());
}

void ScriptManager::printWarning(const std::string& message) {
    logf("[Script WARNING] %s\n", message.c_str());
}

void ScriptManager::printError(const std::string& message) {
    logf("[Script ERROR] %s\n", message.c_str());
}

std::string ScriptManager::getMapName() const {
    Bsp* map = getCurrentMap();
    if (!map) return "";

    return map->name;
}

void ScriptManager::refreshEntityDisplay() {
    Bsp* map = getCurrentMap();
    if (!map) return;
    
    // Refresh the entity rendering
    if (app && app->mapRenderer) {
        for (int i = 0; i < map->ents.size(); i++) {
            app->mapRenderer->refreshEnt(i);
		}
	}
    //app->mapRenderer->preRenderEnts();
    
    logf("[Script] Entity display refreshed\n");
}

std::vector<ScriptEntity*> ScriptManager::getAllEntitiesByClassname(const std::string& classname) {
    std::vector<ScriptEntity*> result;
    Bsp* map = getCurrentMap();
	if (!map) return result;
    
    for (int i = 0; i < (int)map->ents.size(); i++) {
        if (map->ents[i]->getClassname() == classname) {
            result.push_back(getEntity(i));
        }
    }
    return result;
}

float ScriptManager::degToRad(float degrees) {
    return degrees * (float)(M_PI / 180.0);
}

float ScriptManager::radToDeg(float radians) {
    return radians * (float)(180.0 / M_PI);
}

float ScriptManager::getCameraX() const {
    if (!app) return 0.0f;
    return app->cameraOrigin.x;
}

float ScriptManager::getCameraY() const {
    if (!app) return 0.0f;
    return app->cameraOrigin.y;
}

float ScriptManager::getCameraZ() const {
    if (!app) return 0.0f;
    return app->cameraOrigin.z;
}

float ScriptManager::getCameraAnglesPitch() const {
    if (!app) return 0.0f;
    return app->cameraAngles.x;
}

float ScriptManager::getCameraAnglesYaw() const {
    if (!app) return 0.0f;
    return app->cameraAngles.y;
}

float ScriptManager::getCameraAnglesRoll() const {
    if (!app) return 0.0f;
    return app->cameraAngles.z;
}

ScriptVec3 ScriptManager::getCameraPosition() const {
    if (!app) return ScriptVec3();
    return ScriptVec3(app->cameraOrigin.x, app->cameraOrigin.y, app->cameraOrigin.z);
}

ScriptVec3 ScriptManager::getCameraAnglesVec() const {
    if (!app) return ScriptVec3();
    return ScriptVec3(app->cameraAngles.x, app->cameraAngles.y, app->cameraAngles.z);
}

ScriptVec3 ScriptManager::getCameraForward() const {
    if (!app) return ScriptVec3(0, 1, 0);  // Default forward in bspguy is +Y
    
    // Use the same method as Renderer::getMoveDir()
    // bspguy uses: X = pitch, Z = yaw (not Y!)
    vec3 forward, right, up;
    vec3 moveAngles = app->cameraAngles;
    moveAngles.y = 0;  // Y component is not used for camera rotation
    makeVectors(moveAngles, forward, right, up);
    
    return ScriptVec3(forward.x, forward.y, forward.z);
}

void ScriptManager::beginEntityBatch() {
    if (isBatchMode) {
        logf("[Script WARNING] beginEntityBatch() called while already in batch mode\n");
        return;
    }
    isBatchMode = true;
    batchCreatedEntities.clear();
    logf("[Script] Entity batch started\n");
}

void ScriptManager::endEntityBatch() {
    if (!isBatchMode) {
        logf("[Script WARNING] endEntityBatch() called without beginEntityBatch()\n");
        return;
    }
    
    isBatchMode = false;
    
    if (batchCreatedEntities.empty()) {
        logf("[Script] Entity batch ended (no entities created)\n");
        return;
    }
    
    if (!app || !app->mapRenderer || !app->mapRenderer->map) {
        batchCreatedEntities.clear();
        return;
    }
    
    int numBatched = (int)batchCreatedEntities.size();
    
    // The entities are already in the map and modified by the script.
    // We need to create an undo command that knows about these entities.
    // CreateEntitiesCommand stores copies of entities.
    // When undo is called, it removes the last N entities from the map.
    // When redo is called (execute), it adds the copies back.
    
    // Create the undo command with the entities we created
    // Note: CreateEntitiesCommand makes copies, so our pointers remain valid
    CreateEntitiesCommand* cmd = new CreateEntitiesCommand("Script: Create Entities", batchCreatedEntities);
    
    // Push to undo stack (this does NOT execute the command)
    // The entities are already in the map, so we're set up correctly:
    // - Undo will remove the last N entities (which are ours)
    // - Redo (execute) will add them back from the stored copies
    if (app) {
        app->pushUndoCommand(cmd);
    }
    
    logf("[Script] Entity batch ended (%d entities, grouped for undo)\n", numBatched);
    batchCreatedEntities.clear();
}

CScriptArray* ScriptManager::getSelectedEntities() {
    // Get the array type from the engine
    asITypeInfo* arrayType = engine->GetTypeInfoByDecl("array<Entity@>");
    if (!arrayType) {
        logf("[Script ERROR] Could not find array<Entity@> type\n");
        return nullptr;
    }
    
    CScriptArray* arr = CScriptArray::Create(arrayType);
    if (!arr) return nullptr;
    
    Bsp* map = getCurrentMap();
    if (!app || !map || app->pickInfo.ents.empty()) {
        return arr; // Return empty array
    }
    
    if (app->pickInfo.ents.empty()) {
        return arr; // Return empty array
    }
    
    for (int entIdx : app->pickInfo.ents) {
        if (entIdx >= 0 && entIdx < (int)map->ents.size()) {
            ScriptEntity* ent = new ScriptEntity(map->ents[entIdx], map, entIdx);
            arr->InsertLast(&ent);
        }
    }
    
    return arr;
}

int ScriptManager::getSelectedEntityCount() const {
    if (!app) {
        return 0;
    }
    return (int)app->pickInfo.ents.size();
}

void ScriptManager::selectEntity(int index) {
    Bsp* map = getCurrentMap();
    if (!app || !map) return;
    if (index < 0 || index >= (int)map->ents.size()) return;
    app->pickInfo.selectEnt(index);
}

void ScriptManager::deselectEntity(int index) {
    if (!app) {
        return;
    }
    
    app->pickInfo.deselectEnt(index);
}

void ScriptManager::deselectAllEntities() {
    if (!app) return;
    
    app->pickInfo.deselect();
}

bool ScriptManager::isEntitySelected(int index) const {
    if (!app) {
        return false;
    }
    
    return app->pickInfo.isEntSelected(index);
}

// ========================================================================
// LINQ Extensions - Implementation for array<T>
// ========================================================================

CScriptArray* ArrayWhere(asIScriptFunction* func, CScriptArray* arr) {
    asIScriptEngine* engine = arr->GetArrayObjectType()->GetEngine();
    CScriptArray* result = CScriptArray::Create(arr->GetArrayObjectType());
    if (func == 0 || arr->GetSize() == 0) return result;

    asIScriptContext* ctx = asGetActiveContext();
    bool isNested = (ctx && ctx->GetEngine() == engine && ctx->PushState() >= 0);
    if (!isNested) ctx = engine->RequestContext();

    for (asUINT i = 0; i < arr->GetSize(); i++) {
        ctx->Prepare(func);
        ctx->SetArgAddress(0, const_cast<void*>(arr->At(i)));
        if (ctx->Execute() == asEXECUTION_FINISHED) {
            if (*(bool*)(ctx->GetAddressOfReturnValue())) {
                result->InsertLast(const_cast<void*>(arr->At(i)));
            }
        }
        else break;
    }

    if (isNested) ctx->PopState();
    else engine->ReturnContext(ctx);
    return result;
}

bool ArrayAny(asIScriptFunction* func, CScriptArray* arr) {
    if (func == 0 || arr->GetSize() == 0) return false;
    asIScriptEngine* engine = arr->GetArrayObjectType()->GetEngine();
    asIScriptContext* ctx = asGetActiveContext();
    bool isNested = (ctx && ctx->GetEngine() == engine && ctx->PushState() >= 0);
    if (!isNested) ctx = engine->RequestContext();

    bool found = false;
    for (asUINT i = 0; i < arr->GetSize(); i++) {
        ctx->Prepare(func);
        ctx->SetArgAddress(0, const_cast<void*>(arr->At(i)));
        if (ctx->Execute() == asEXECUTION_FINISHED) {
            if (*(bool*)(ctx->GetAddressOfReturnValue())) {
                found = true;
                break;
            }
        }
        else break;
    }

    if (isNested) ctx->PopState();
    else engine->ReturnContext(ctx);
    return found;
}

bool ArrayAll(asIScriptFunction* func, CScriptArray* arr) {
    if (func == 0) return false;
    if (arr->GetSize() == 0) return true;
    asIScriptEngine* engine = arr->GetArrayObjectType()->GetEngine();
    asIScriptContext* ctx = asGetActiveContext();
    bool isNested = (ctx && ctx->GetEngine() == engine && ctx->PushState() >= 0);
    if (!isNested) ctx = engine->RequestContext();

    bool allMatch = true;
    for (asUINT i = 0; i < arr->GetSize(); i++) {
        ctx->Prepare(func);
        ctx->SetArgAddress(0, const_cast<void*>(arr->At(i)));
        if (ctx->Execute() == asEXECUTION_FINISHED) {
            if (!(*(bool*)(ctx->GetAddressOfReturnValue()))) {
                allMatch = false;
                break;
            }
        }
        else { allMatch = false; break; }
    }

    if (isNested) ctx->PopState();
    else engine->ReturnContext(ctx);
    return allMatch;
}

int ArrayCount(asIScriptFunction* func, CScriptArray* arr) {
    if (func == 0 || arr->GetSize() == 0) return 0;
    asIScriptEngine* engine = arr->GetArrayObjectType()->GetEngine();
    asIScriptContext* ctx = asGetActiveContext();
    bool isNested = (ctx && ctx->GetEngine() == engine && ctx->PushState() >= 0);
    if (!isNested) ctx = engine->RequestContext();

    int count = 0;
    for (asUINT i = 0; i < arr->GetSize(); i++) {
        ctx->Prepare(func);
        ctx->SetArgAddress(0, const_cast<void*>(arr->At(i)));
        if (ctx->Execute() == asEXECUTION_FINISHED) {
            if (*(bool*)(ctx->GetAddressOfReturnValue())) count++;
        }
        else break;
    }

    if (isNested) ctx->PopState();
    else engine->ReturnContext(ctx);
    return count;
}

int ArrayFirstIndex(asIScriptFunction* func, CScriptArray* arr) {
    if (func == 0 || arr->GetSize() == 0) return -1;
    asIScriptEngine* engine = arr->GetArrayObjectType()->GetEngine();
    asIScriptContext* ctx = asGetActiveContext();
    bool isNested = (ctx && ctx->GetEngine() == engine && ctx->PushState() >= 0);
    if (!isNested) ctx = engine->RequestContext();

    int matchIdx = -1;
    for (asUINT i = 0; i < arr->GetSize(); i++) {
        ctx->Prepare(func);
        ctx->SetArgAddress(0, const_cast<void*>(arr->At(i)));
        if (ctx->Execute() == asEXECUTION_FINISHED) {
            if (*(bool*)(ctx->GetAddressOfReturnValue())) {
                matchIdx = (int)i;
                break;
            }
        }
        else break;
    }

    if (isNested) ctx->PopState();
    else engine->ReturnContext(ctx);
    return matchIdx;
}

void* ArrayFirst(asIScriptFunction* func, CScriptArray* arr) {
    int idx = ArrayFirstIndex(func, arr);
    if (idx >= 0) return const_cast<void*>(arr->At(idx));
    asIScriptContext* ctx = asGetActiveContext();
    if (ctx) ctx->SetException("Sequence contains no matching element");
    return 0;
}

void* ArrayFirstOrDefault(asIScriptFunction* func, void* defaultValue, CScriptArray* arr) {
    int idx = ArrayFirstIndex(func, arr);
    return (idx >= 0) ? const_cast<void*>(arr->At(idx)) : defaultValue;
}

int ArrayLastIndex(asIScriptFunction* func, CScriptArray* arr) {
    if (func == 0 || arr->GetSize() == 0) return -1;
    asIScriptEngine* engine = arr->GetArrayObjectType()->GetEngine();
    asIScriptContext* ctx = asGetActiveContext();
    bool isNested = (ctx && ctx->GetEngine() == engine && ctx->PushState() >= 0);
    if (!isNested) ctx = engine->RequestContext();

    int lastIdx = -1;
    for (int i = (int)arr->GetSize() - 1; i >= 0; i--) {
        ctx->Prepare(func);
        ctx->SetArgAddress(0, const_cast<void*>(arr->At(i)));
        if (ctx->Execute() == asEXECUTION_FINISHED) {
            if (*(bool*)(ctx->GetAddressOfReturnValue())) {
                lastIdx = i;
                break;
            }
        }
        else break;
    }

    if (isNested) ctx->PopState();
    else engine->ReturnContext(ctx);
    return lastIdx;
}

void* ArrayLast(asIScriptFunction* func, CScriptArray* arr) {
    int idx = ArrayLastIndex(func, arr);
    if (idx >= 0) return const_cast<void*>(arr->At(idx));
    asIScriptContext* ctx = asGetActiveContext();
    if (ctx) ctx->SetException("Sequence contains no matching element");
    return 0;
}

void ScriptManager::registerTypes() {
    int r;

    // ========================================================================
    // Core Types - Basic engine entity structures
    // ========================================================================

    // Register ScriptEntity as a reference type
    r = engine->RegisterObjectType("Entity", 0, asOBJ_REF); assert(r >= 0);

    // Register reference counting behaviors for Entity
    r = engine->RegisterObjectBehaviour("Entity", asBEHAVE_ADDREF, "void f()",
        asMETHOD(ScriptEntity, AddRef), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("Entity", asBEHAVE_RELEASE, "void f()",
        asMETHOD(ScriptEntity, Release), asCALL_THISCALL); assert(r >= 0);
}

void ScriptManager::registerMathTypes() {
    int r;

    // ========================================================================
    // Vec3 Type - 3D Vector representation and operations
    // ========================================================================

    // Register Vec3 as a value type
    r = engine->RegisterObjectType("Vec3", sizeof(ScriptVec3), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<ScriptVec3>()); assert(r >= 0);

    // Register Vec3 constructors
    r = engine->RegisterObjectBehaviour("Vec3", asBEHAVE_CONSTRUCT, "void f()",
        asFUNCTIONPR([](void* mem) { new(mem) ScriptVec3(); }, (void*), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("Vec3", asBEHAVE_CONSTRUCT, "void f(float, float, float)",
        asFUNCTIONPR([](void* mem, float x, float y, float z) { new(mem) ScriptVec3(x, y, z); }, (void*, float, float, float), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("Vec3", asBEHAVE_CONSTRUCT, "void f(const Vec3 &in)",
        asFUNCTIONPR([](void* mem, const ScriptVec3& other) { new(mem) ScriptVec3(other); }, (void*, const ScriptVec3&), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Register Vec3 properties
    r = engine->RegisterObjectProperty("Vec3", "float x", asOFFSET(ScriptVec3, x)); assert(r >= 0);
    r = engine->RegisterObjectProperty("Vec3", "float y", asOFFSET(ScriptVec3, y)); assert(r >= 0);
    r = engine->RegisterObjectProperty("Vec3", "float z", asOFFSET(ScriptVec3, z)); assert(r >= 0);

    // Register Vec3 math operators (Must remain camelCase for AngelScript operator overloading rules)
    r = engine->RegisterObjectMethod("Vec3", "Vec3 &opAssign(const Vec3 &in)", asMETHOD(ScriptVec3, operator=), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Vec3", "Vec3 opAdd(const Vec3 &in) const", asMETHODPR(ScriptVec3, operator+, (const ScriptVec3&) const, ScriptVec3), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Vec3", "Vec3 opSub(const Vec3 &in) const", asMETHODPR(ScriptVec3, operator-, (const ScriptVec3&) const, ScriptVec3), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Vec3", "Vec3 opMul(float) const", asMETHODPR(ScriptVec3, operator*, (float) const, ScriptVec3), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Vec3", "Vec3 opDiv(float) const", asMETHODPR(ScriptVec3, operator/, (float) const, ScriptVec3), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Vec3", "Vec3 opNeg() const", asMETHODPR(ScriptVec3, operator-, () const, ScriptVec3), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Vec3", "bool opEquals(const Vec3 &in) const", asMETHOD(ScriptVec3, operator==), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Vec3", "Vec3 &opAddAssign(const Vec3 &in)", asMETHOD(ScriptVec3, operator+=), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Vec3", "Vec3 &opSubAssign(const Vec3 &in)", asMETHOD(ScriptVec3, operator-=), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Vec3", "Vec3 &opMulAssign(float)", asMETHODPR(ScriptVec3, operator*=, (float), ScriptVec3&), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Vec3", "Vec3 &opDivAssign(float)", asMETHODPR(ScriptVec3, operator/=, (float), ScriptVec3&), asCALL_THISCALL); assert(r >= 0);

    // Register Vec3 utility methods (Capitalized)
    r = engine->RegisterObjectMethod("Vec3", "float Length() const", asMETHOD(ScriptVec3, Length), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Vec3", "float Dot(const Vec3 &in) const", asMETHOD(ScriptVec3, Dot), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Vec3", "Vec3 Cross(const Vec3 &in) const", asMETHOD(ScriptVec3, Cross), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Vec3", "void Normalize()", asMETHOD(ScriptVec3, Normalize), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Vec3", "float Distance(const Vec3 &in) const", asMETHOD(ScriptVec3, Distance), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Vec3", "Vec3 Lerp(const Vec3 &in, float) const", asMETHOD(ScriptVec3, Lerp), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Vec3", "string ToString() const", asMETHOD(ScriptVec3, ToString), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Vec3", "string ToKeyvalueString(bool = false, const string &in = \" \", const string &in = \" \", const string &in = \"\") const", asMETHOD(ScriptVec3, ToKeyvalueString), asCALL_THISCALL); assert(r >= 0);

    // Set namespace to Vec3 for static factories
    r = engine->SetDefaultNamespace("Vec3"); assert(r >= 0);

    // Register Vec3 static factory functions (Capitalized)
    r = engine->RegisterGlobalFunction("Vec3 FromString(const string &in)", asFUNCTION(ScriptVec3::FromString), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("Vec3 Zero()", asFUNCTION(ScriptVec3::Zero), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("Vec3 One()", asFUNCTION(ScriptVec3::One), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("Vec3 Up()", asFUNCTION(ScriptVec3::Up), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("Vec3 Forward()", asFUNCTION(ScriptVec3::Forward), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("Vec3 Right()", asFUNCTION(ScriptVec3::Right), asCALL_CDECL); assert(r >= 0);

    // Reset namespace to global
    r = engine->SetDefaultNamespace(""); assert(r >= 0);

    // ========================================================================
    // RGB Type - Color representation and operations
    // ========================================================================

    // Register RGB as a value type
    r = engine->RegisterObjectType("RGB", sizeof(ScriptRGB), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<ScriptRGB>()); assert(r >= 0);

    // Register RGB constructors
    r = engine->RegisterObjectBehaviour("RGB", asBEHAVE_CONSTRUCT, "void f()",
        asFUNCTIONPR([](void* mem) { new(mem) ScriptRGB(); }, (void*), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("RGB", asBEHAVE_CONSTRUCT, "void f(int, int, int)",
        asFUNCTIONPR([](void* mem, int r, int g, int b) { new(mem) ScriptRGB(r, g, b); }, (void*, int, int, int), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = engine->RegisterObjectBehaviour("RGB", asBEHAVE_CONSTRUCT, "void f(const RGB &in)",
        asFUNCTIONPR([](void* mem, const ScriptRGB& other) { new(mem) ScriptRGB(other); }, (void*, const ScriptRGB&), void), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // Register RGB properties
    r = engine->RegisterObjectProperty("RGB", "int r", asOFFSET(ScriptRGB, r)); assert(r >= 0);
    r = engine->RegisterObjectProperty("RGB", "int g", asOFFSET(ScriptRGB, g)); assert(r >= 0);
    r = engine->RegisterObjectProperty("RGB", "int b", asOFFSET(ScriptRGB, b)); assert(r >= 0);

    // Register RGB operators
    r = engine->RegisterObjectMethod("RGB", "RGB &opAssign(const RGB &in)", asMETHOD(ScriptRGB, operator=), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("RGB", "bool opEquals(const RGB &in) const", asMETHOD(ScriptRGB, operator==), asCALL_THISCALL); assert(r >= 0);

    // Register RGB utility methods (Capitalized)
    r = engine->RegisterObjectMethod("RGB", "RGB Lerp(const RGB &in, float) const", asMETHOD(ScriptRGB, Lerp), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("RGB", "void Clamp()", asMETHOD(ScriptRGB, Clamp), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("RGB", "string ToString() const", asMETHOD(ScriptRGB, ToString), asCALL_THISCALL); assert(r >= 0);

    // Set namespace to RGB for static factories
    r = engine->SetDefaultNamespace("RGB"); assert(r >= 0);

    // Register RGB static factory functions (Capitalized)
    r = engine->RegisterGlobalFunction("RGB FromString(const string &in)", asFUNCTION(ScriptRGB::FromString), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("RGB Default()", asFUNCTION(ScriptRGB::Default), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("RGB White()", asFUNCTION(ScriptRGB::White), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("RGB Black()", asFUNCTION(ScriptRGB::Black), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("RGB Red()", asFUNCTION(ScriptRGB::Red), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("RGB Green()", asFUNCTION(ScriptRGB::Green), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("RGB Blue()", asFUNCTION(ScriptRGB::Blue), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("RGB Yellow()", asFUNCTION(ScriptRGB::Yellow), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("RGB Cyan()", asFUNCTION(ScriptRGB::Cyan), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("RGB Magenta()", asFUNCTION(ScriptRGB::Magenta), asCALL_CDECL); assert(r >= 0);

    // Reset namespace to global
    r = engine->SetDefaultNamespace(""); assert(r >= 0);

    // ========================================================================
    // Math Namespace - Math utilities and formulas
    // ========================================================================

    // Set namespace to Math
    r = engine->SetDefaultNamespace("Math"); assert(r >= 0);

    // Register general math functions (Capitalized exposed names, original C++ function pointers)
    r = engine->RegisterGlobalFunction("float DegToRad(float)",
        asFUNCTION(Script_degToRad), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("float RadToDeg(float)",
        asFUNCTION(Script_radToDeg), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("float Round(float)",
        asFUNCTION(Script_round), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("float Min(float, float)",
        asFUNCTION(Script_min), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("float Max(float, float)",
        asFUNCTION(Script_max), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("float Clamp(float, float, float)",
        asFUNCTION(Script_clamp), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("float Lerp(float, float, float)",
        asFUNCTION(Script_lerp), asCALL_CDECL); assert(r >= 0);

    // Reset namespace to global
    r = engine->SetDefaultNamespace(""); assert(r >= 0);
}

void ScriptManager::registerGlobalFunctions() {
    int r;

    // ========================================================================
    // Global Functions - Common engine utilities
    // ========================================================================

    // Register basic print functions
    r = engine->RegisterGlobalFunction("void print(const string &in)",
        asFUNCTION(Script_print), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("void printWarning(const string &in)",
        asFUNCTION(Script_printWarning), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("void printError(const string &in)",
        asFUNCTION(Script_printError), asCALL_CDECL); assert(r >= 0);

    // ========================================================================
    // Convert Namespace - Type conversions
    // ========================================================================

    // Set namespace to Convert
    r = engine->SetDefaultNamespace("Convert"); assert(r >= 0);

    // Register string to number and number to string converters
    r = engine->RegisterGlobalFunction("string toString(int)",
        asFUNCTION(Script_intToString), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("string toString(float)",
        asFUNCTION(Script_floatToString), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("int toInt(const string &in)",
        asFUNCTION(Script_stringToInt), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("float toFloat(const string &in)",
        asFUNCTION(Script_stringToFloat), asCALL_CDECL); assert(r >= 0);

    // Reset namespace to global
    r = engine->SetDefaultNamespace(""); assert(r >= 0);

    // ========================================================================
    // Camera Namespace - Viewport camera access
    // ========================================================================

    // Set namespace to Camera
    r = engine->SetDefaultNamespace("Camera"); assert(r >= 0);

    // Register individual camera coordinate getters
    r = engine->RegisterGlobalFunction("float getX()",
        asFUNCTION(Script_getCameraX), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("float getY()",
        asFUNCTION(Script_getCameraY), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("float getZ()",
        asFUNCTION(Script_getCameraZ), asCALL_CDECL); assert(r >= 0);

    // Register individual camera rotation getters
    r = engine->RegisterGlobalFunction("float getPitch()",
        asFUNCTION(Script_getCameraPitch), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("float getYaw()",
        asFUNCTION(Script_getCameraYaw), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("float getRoll()",
        asFUNCTION(Script_getCameraRoll), asCALL_CDECL); assert(r >= 0);

    // Register Vec3 camera getters
    r = engine->RegisterGlobalFunction("Vec3 getPos()",
        asFUNCTION(Script_getCameraPos), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("Vec3 getAngles()",
        asFUNCTION(Script_getCameraAngles), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("Vec3 getForward()",
        asFUNCTION(Script_getCameraForward), asCALL_CDECL); assert(r >= 0);

    // Reset namespace to global
    r = engine->SetDefaultNamespace(""); assert(r >= 0);

    // ========================================================================
    // Map Namespace - Global map and entity management
    // ========================================================================

    // Set namespace to Map
    r = engine->SetDefaultNamespace("Map"); assert(r >= 0);

    // Register general map properties
    r = engine->RegisterGlobalFunction("string getName()",
        asFUNCTION(Script_getMapName), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("int getEntityCount()",
        asFUNCTION(Script_getEntityCount), asCALL_CDECL); assert(r >= 0);

    // Register entity lookup functions
    r = engine->RegisterGlobalFunction("Entity@ getEntity(int)",
        asFUNCTION(Script_getEntity), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("Entity@ findByTargetname(const string &in)",
        asFUNCTION(Script_getEntityByTargetname), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("Entity@ findByClassname(const string &in)",
        asFUNCTION(Script_getEntityByClassname), asCALL_CDECL); assert(r >= 0);

    // Register entity lifecycle functions
    r = engine->RegisterGlobalFunction("Entity@ createEntity(const string &in)",
        asFUNCTION(Script_createEntity), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("void deleteEntity(int)",
        asFUNCTION(Script_deleteEntity), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("void refresh()",
        asFUNCTION(Script_refreshEntities), asCALL_CDECL); assert(r >= 0);

    // Reset namespace to global
    r = engine->SetDefaultNamespace(""); assert(r >= 0);

    // ========================================================================
    // Editor Namespace - Undo, selection, and tools
    // ========================================================================

    // Set namespace to Editor
    r = engine->SetDefaultNamespace("Editor"); assert(r >= 0);

    // Register batch operations for undo grouping
    r = engine->RegisterGlobalFunction("void beginBatch()",
        asFUNCTION(Script_beginEntityBatch), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("void endBatch()",
        asFUNCTION(Script_endEntityBatch), asCALL_CDECL); assert(r >= 0);

    // Register entity selection tools
    r = engine->RegisterGlobalFunction("array<Entity@>@ getSelectedEntities()",
        asFUNCTION(Script_getSelectedEntities), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("int getSelectedCount()",
        asFUNCTION(Script_getSelectedEntityCount), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("void selectEntity(int)",
        asFUNCTION(Script_selectEntity), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("void deselectEntity(int)",
        asFUNCTION(Script_deselectEntity), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("void deselectAll()",
        asFUNCTION(Script_deselectAllEntities), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("bool isSelected(int)",
        asFUNCTION(Script_isEntitySelected), asCALL_CDECL); assert(r >= 0);

    // Reset namespace to global
    r = engine->SetDefaultNamespace(""); assert(r >= 0);
}

void ScriptManager::registerEntityMethods() {
    int r;

    // ========================================================================
    // Entity Methods - Manipulation and data access for Map Entities
    // ========================================================================

    // Register Keyvalue methods
    // Nota: Se asume que CScriptDictionary está registrado en AngelScript como "dictionary"
    r = engine->RegisterObjectMethod("Entity", "dictionary@ GetKeyValues() const",
        asMETHOD(ScriptEntity, GetKeyValues), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Entity", "string GetKeyValue(const string &in) const",
        asMETHOD(ScriptEntity, GetKeyValue), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Entity", "void SetKeyValue(const string &in, const string &in)",
        asMETHOD(ScriptEntity, SetKeyValue), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Entity", "bool HasKeyValue(const string &in) const",
        asMETHOD(ScriptEntity, HasKeyValue), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Entity", "void RemoveKeyValue(const string &in)",
        asMETHOD(ScriptEntity, RemoveKeyValue), asCALL_THISCALL); assert(r >= 0);

    // Register Classname and Targetname
    r = engine->RegisterObjectMethod("Entity", "string GetClassname() const",
        asMETHOD(ScriptEntity, GetClassname), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Entity", "void SetClassname(const string &in)",
        asMETHOD(ScriptEntity, SetClassname), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Entity", "string GetTargetname() const",
        asMETHOD(ScriptEntity, GetTargetname), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Entity", "void SetTargetname(const string &in)",
        asMETHOD(ScriptEntity, SetTargetname), asCALL_THISCALL); assert(r >= 0);

    // Register Position and Orientation 
    // Nota: Se asume que ScriptVec3 está registrado en AngelScript como "Vec3"
    r = engine->RegisterObjectMethod("Entity", "Vec3 GetOrigin() const",
        asMETHOD(ScriptEntity, GetOrigin), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Entity", "void SetOrigin(const Vec3 &in)",
        asMETHOD(ScriptEntity, SetOrigin), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Entity", "Vec3 GetAngles() const",
        asMETHOD(ScriptEntity, GetAngles), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Entity", "void SetAngles(const Vec3 &in)",
        asMETHOD(ScriptEntity, SetAngles), asCALL_THISCALL); assert(r >= 0);
    
    // Register BSP model related methods
    r = engine->RegisterObjectMethod("Entity", "int GetBspModelIdx() const",
        asMETHOD(ScriptEntity, GetBspModelIdx), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Entity", "bool IsBspModel() const",
        asMETHOD(ScriptEntity, IsBspModel), asCALL_THISCALL); assert(r >= 0);

    // Register internal identification methods
    r = engine->RegisterObjectMethod("Entity", "int GetIndex() const",
        asMETHOD(ScriptEntity, GetIndex), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Entity", "bool IsValid() const",
        asMETHOD(ScriptEntity, IsValid), asCALL_THISCALL); assert(r >= 0);

    // Register spatial calculation methods
    r = engine->RegisterObjectMethod("Entity", "float DistanceTo(const Entity &in) const",
        asMETHOD(ScriptEntity, DistanceTo), asCALL_THISCALL); assert(r >= 0);
    r = engine->RegisterObjectMethod("Entity", "bool Intersects(const Entity &in) const",
        asMETHOD(ScriptEntity, Intersects), asCALL_THISCALL); assert(r >= 0);
}

void ScriptManager::registerArrayExtensions() {
    int r;

    // ========================================================================
    // Array Extensions - LINQ standard query operators
    // ========================================================================

    // Register a generic function definition for the filter callbacks
    r = engine->RegisterFuncdef("bool array<T>::filter(const T&in if_handle_then_const)"); assert(r >= 0);

    // Register Where (Filtering)
    r = engine->RegisterObjectMethod("array<T>", "array<T>@ Where(const filter &in) const",
        asFUNCTION(ArrayWhere), asCALL_CDECL_OBJLAST); assert(r >= 0);

    // Register Boolean checks
    r = engine->RegisterObjectMethod("array<T>", "bool Any(const filter &in) const",
        asFUNCTION(ArrayAny), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "bool All(const filter &in) const",
        asFUNCTION(ArrayAll), asCALL_CDECL_OBJLAST); assert(r >= 0);

    // Register Counting
    r = engine->RegisterObjectMethod("array<T>", "int Count(const filter &in) const",
        asFUNCTION(ArrayCount), asCALL_CDECL_OBJLAST); assert(r >= 0);

    // Register First/FirstOrDefault
    r = engine->RegisterObjectMethod("array<T>", "int FirstIndex(const filter &in) const",
        asFUNCTION(ArrayFirstIndex), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "T& First(const filter &in) const",
        asFUNCTION(ArrayFirst), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "T& FirstOrDefault(const filter &in, const T&in) const",
        asFUNCTION(ArrayFirstOrDefault), asCALL_CDECL_OBJLAST); assert(r >= 0);

    // Register Last/LastIndex
    r = engine->RegisterObjectMethod("array<T>", "int LastIndex(const filter &in) const",
        asFUNCTION(ArrayLastIndex), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = engine->RegisterObjectMethod("array<T>", "T& Last(const filter &in) const",
        asFUNCTION(ArrayLast), asCALL_CDECL_OBJLAST); assert(r >= 0);
}

void ScriptManager::generateScriptPredefined(const std::string& path) {
    std::ofstream stream(path);
    if (!stream.is_open()) return;

    stream << "// ========================================================================\n";
    stream << "// BSPGUY ANGELSCRIPT API (AUTO-GENERATED)\n";
    stream << "// ========================================================================\n\n";

    auto SafeStr = [](const char* str) -> std::string {
        return str ? str : "/* anonymous */";
        };

    // 1. Enums
    for (int i = 0; i < engine->GetEnumCount(); i++) {
        const auto e = engine->GetEnumByIndex(i);
        if (!e) continue;
        std::string ns = SafeStr(e->GetNamespace());
        if (!ns.empty()) stream << "namespace " << ns << " {\n";
        stream << "enum " << SafeStr(e->GetName()) << " {\n";
        for (int j = 0; j < e->GetEnumValueCount(); ++j) {
            stream << "\t" << SafeStr(e->GetEnumValueByIndex(j, nullptr));
            if (j < e->GetEnumValueCount() - 1) stream << ",";
            stream << "\n";
        }
        stream << "}\n";
        if (!ns.empty()) stream << "}\n";
    }

    for (int i = 0; i < engine->GetObjectTypeCount(); i++) {
        const auto t = engine->GetObjectTypeByIndex(i);
        if (!t) continue;

        std::string name = SafeStr(t->GetName());
        if (name == "/* anonymous */" || name.find("$") != std::string::npos) continue;

        std::string ns = SafeStr(t->GetNamespace());
        if (!ns.empty()) stream << "namespace " << ns << " {\n";

        stream << "class " << name;
        if (t->GetSubTypeCount() > 0) {
            stream << "<";
            for (asUINT sub = 0; sub < t->GetSubTypeCount(); ++sub) {
                if (sub > 0) stream << ", ";
                stream << SafeStr(engine->GetTypeDeclaration(t->GetSubTypeId(sub), true));
            }
            stream << ">";
        }
        stream << " {\n";

        for (asUINT j = 0; j < t->GetBehaviourCount(); ++j) {
            asEBehaviours behaviours;
            const auto f = t->GetBehaviourByIndex(j, &behaviours);
            if (f && (behaviours == asBEHAVE_CONSTRUCT || behaviours == asBEHAVE_DESTRUCT)) {
                stream << "\t" << SafeStr(f->GetDeclaration(false, true, true)) << ";\n";
            }
        }
        for (asUINT j = 0; j < t->GetMethodCount(); ++j) {
            const auto m = t->GetMethodByIndex(j);
            if (m) stream << "\t" << SafeStr(m->GetDeclaration(false, true, true)) << ";\n";
        }
        for (asUINT j = 0; j < t->GetPropertyCount(); ++j) {
            stream << "\t" << SafeStr(t->GetPropertyDeclaration(j, true)) << ";\n";
        }
        for (asUINT j = 0; j < t->GetChildFuncdefCount(); ++j) {
            auto funcdef = t->GetChildFuncdef(j);
            if (funcdef && funcdef->GetFuncdefSignature()) {
                stream << "\tfuncdef " << SafeStr(funcdef->GetFuncdefSignature()->GetDeclaration(false, false, false)) << ";\n";
            }
        }
        stream << "}\n";
        if (!ns.empty()) stream << "}\n";
    }

    std::map<std::string, std::vector<std::string>> groupedFunctions;

    for (int i = 0; i < engine->GetGlobalFunctionCount(); i++) {
        const auto f = engine->GetGlobalFunctionByIndex(i);
        if (!f) continue;
        std::string ns = SafeStr(f->GetNamespace());
        groupedFunctions[ns].push_back(SafeStr(f->GetDeclaration(false, false, true)));
    }

    for (const auto& group : groupedFunctions) {
        std::string ns = group.first;

        if (ns.empty()) {
            for (const auto& func : group.second) {
                stream << func << ";\n";
            }
        }
        else {
            stream << "namespace " << ns << " {\n";
            for (const auto& func : group.second) {
                stream << "\t" << func << ";\n";
            }
            stream << "}\n";
        }
    }

    // 4. Typedefs
    for (int i = 0; i < engine->GetTypedefCount(); ++i) {
        const auto type = engine->GetTypedefByIndex(i);
        if (!type) continue;
        std::string ns = SafeStr(type->GetNamespace());
        if (!ns.empty()) stream << "namespace " << ns << " {\n";
        stream << "typedef " << SafeStr(engine->GetTypeDeclaration(type->GetUnderlyingTypeId(), true)) << " " << SafeStr(type->GetName()) << ";\n";
        if (!ns.empty()) stream << "}\n";
    }

    stream.close();
}