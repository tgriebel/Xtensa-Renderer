#include <string>
#include <fstream>
#include <assert.h>
#if defined _MSC_VER
#include<direct.h>
#endif
#include <vector>
#include <algorithm>
#include <filesystem>

using namespace std;

namespace SysCore
{

bool FileExists( const string& path )
{
	ifstream stream( path.c_str() );
	const bool exist = stream.good();
	stream.close();
	return exist;
}


bool MakeDirectory( const string& path )
{
#if defined _MSC_VER
	return ( _mkdir( path.c_str() ) == 0 );
#else
	assert( 0 );
	return false;
#endif
}


bool CloneFile( const string& srcPath, const string& dstPath )
{
	error_code ec;
	return filesystem::copy_file( srcPath, dstPath, ec );
}


void SplitFileName( const string& path, string& fileName, string& ext )
{
	size_t extPos = path.find_last_of( "." );

	fileName = path.substr( 0, extPos );
	ext = path.substr( extPos + 1, path.length() );
}


void SplitPath( const string& path, string& directory, string& fileName )
{
	size_t dirPos = path.find_last_of( "\\" ) + 1;
	directory = path.substr( 0, dirPos );
	fileName = path.substr( dirPos, path.length() );
}


string MakeRelative( const string& path, const string& root )
{
	return filesystem::relative( path, filesystem::absolute( root ) ).generic_string();
}


void LeftTrim( std::string& s )
{
	s.erase( s.begin(), std::find_if( s.begin(), s.end(), []( unsigned char ch ) {
		return !std::isspace( ch );
		} ) );
}


std::string LeftTrim( const std::string& s )
{
	std::string trimmed = s;
	LeftTrim( trimmed );
	return trimmed;
}


void RightTrim( std::string& s )
{
	s.erase( std::find_if( s.rbegin(), s.rend(), []( unsigned char ch ) {
		return !std::isspace( ch );
		} ).base(), s.end() );
}


std::string RightTrim( const std::string& s )
{
	std::string trimmed = s;
	RightTrim( trimmed );
	return trimmed;
}


void Trim( std::string& s )
{
	LeftTrim( s );
	RightTrim( s );
}


std::string Trim( const std::string& s )
{
	std::string trimmed = s;
	Trim( trimmed );
	return trimmed;
}


void ToLower( std::string& s )
{
	std::transform( s.begin(), s.end(), s.begin(), []( unsigned char c ) {
		return std::tolower( c );
		} );
}


std::string ToLower( const std::string& s )
{
	std::string lower = s;
	ToLower( lower );
	return lower;
}


void ToUpper( std::string& s )
{
	std::transform( s.begin(), s.end(), s.begin(), []( unsigned char c ) {
		return std::toupper( c );
		} );
}


std::string ToUpper( const std::string& s )
{
	std::string upper = s;
	ToUpper( upper );
	return upper;
}


bool Equals( const std::string& str0, const std::string& str1 )
{
	return str0.compare( str1 ) == 0;
}


bool HasPrefix( const std::string& str0, const std::string& str1 )
{
	return str0.find( str1 ) == 0;
}


bool HasSuffix( const std::string& str0, const std::string& str1 )
{
	return str0.find( str1 ) == ( str0.size() - str1.size() );
}


std::vector<char> ReadBinaryFile( const std::string& filename )
{
	std::ifstream file( filename, std::ios::ate | std::ios::binary );

	if( !file.is_open() ) {
		throw std::runtime_error( "Failed to open file!" );
	}

	size_t fileSize = ( size_t )file.tellg();
	std::vector<char> buffer( fileSize );

	file.seekg( 0 );
	file.read( buffer.data(), fileSize );
	file.close();

	return buffer;
}


std::vector<char> ReadTextFile( const std::string& filename )
{
	std::ifstream file( filename, std::ios::ate );

	if( !file.is_open() ) {
		throw std::runtime_error( "Failed to open file!" );
	}

	size_t fileSize = ( size_t )file.tellg();
	std::vector<char> buffer( fileSize );

	file.seekg( 0 );
	file.read( buffer.data(), fileSize );
	file.close();

	return buffer;
}

}
