#pragma once

#include <vector>
#include <string>
#include <fstream>

#include <syscore/serializer.h>

#include "../globals/common.h"

class Image;
class Model;
class Material;
class AssetManager;

template<class T>
class Asset;

bool LoadImage( const char* texturePath, const bool isLinearColor, Image& texture );
bool LoadImageHDR( const char* texturePath, Image& texture );
bool LoadCubeMapImage( const char* textureBasePath, const char* ext, Image& texture );
bool WriteImage( const char* path, const Image& image );
bool LoadMaterialObj( AssetManager& assets, const std::string& fileName, const std::string& materialPath, const std::string& texturePath, Material& material );
bool LoadRawModelModelObj( AssetManager& assets, const std::string& fileName, const std::string& modelPath, const std::string& texturePath, Model& model );
bool LoadRawModelGLTF( AssetManager& assets, const std::string& fileName, const std::string& modelPath, const std::string& texturePath, Model& model );
bool LoadModel( AssetManager& assets, const hdl_t& hdl, const std::string& bakePath, const std::string& modelPath, const std::string& ext );
bool WriteModel( Asset<Model>* model, const std::string& fileName );
