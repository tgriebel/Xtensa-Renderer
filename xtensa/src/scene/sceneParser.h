#pragma once
#include <string>

class Scene;
class AssetManager;

typedef void sceneInitializerCallback_t( const std::string type, Scene** scene );

// A scene folder must contain a <name>.json with the same name as the folder,
// e.g. "chess" -> "chess/chess.json".
std::string SceneJsonPath( const std::string& sceneName );

void LoadScene( std::string fileName, Scene** scene, AssetManager* assets, sceneInitializerCallback_t* sceneInitializer );