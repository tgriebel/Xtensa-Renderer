#pragma once
#include <string>
#include <vector>

namespace SysCore
{

bool				FileExists( const std::string& path );
bool				MakeDirectory( const std::string& path );
bool				CloneFile( const std::string& srcPath, const std::string& dstPath );
void				SplitFileName( const std::string& path, std::string& fileName, std::string& ext );
void				SplitPath( const std::string& path, std::string& directory, std::string& fileName );
std::string			MakeRelative( const std::string& path, const std::string& root );
void				LeftTrim( std::string& s );
std::string			LeftTrim( const std::string& s );
void				RightTrim( std::string& s );
std::string			RightTrim( const std::string& s );
void				Trim( std::string& s );
std::string			Trim( const std::string& s );
bool				Equals( const std::string& str0, const std::string& str1 );
void				ToLower( std::string& s );
std::string			ToLower( const std::string& s );
void				ToUpper( std::string& s );
std::string			ToUpper( const std::string& s );
bool				HasPrefix( const std::string& str0, const std::string& str1 );
bool				HasSuffix( const std::string& str0, const std::string& str1 );
std::vector<char>	ReadTextFile( const std::string& filename );
std::vector<char>	ReadBinaryFile( const std::string& filename );

}
