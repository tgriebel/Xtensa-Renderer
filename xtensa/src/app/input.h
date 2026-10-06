#pragma once

#include "../globals/common.h"

#define KEY( k ) KEY_##k = (#k[0])

enum key_t
{
	KEY( 0 ),
	KEY( 1 ),
	KEY( 2 ),
	KEY( 3 ),
	KEY( 4 ),
	KEY( 5 ),
	KEY( 6 ),
	KEY( 7 ),
	KEY( 8 ),
	KEY( 9 ),
	KEY( A ),
	KEY( B ),
	KEY( C ),
	KEY( D ),
	KEY( E ),
	KEY( F ),
	KEY( G ),
	KEY( H ),
	KEY( I ),
	KEY( J ),
	KEY( K ),
	KEY( L ),
	KEY( M ),
	KEY( N ),
	KEY( O ),
	KEY( P ),
	KEY( Q ),
	KEY( R ),
	KEY( S ),
	KEY( T ),
	KEY( U ),
	KEY( V ),
	KEY( W ),
	KEY( X ),
	KEY( Y ),
	KEY( Z ),
	KEY_UP_ARROW,
	KEY_DOWN_ARROW,
	KEY_LEFT_ARROW,
	KEY_RIGHT_ARROW,
	KEY_LEFT_ALT,
	KEY_RIGHT_ALT,
	KEY_LEFT_CTRL,
	KEY_RIGHT_CTRL,
	KEY_LEFT_SHIFT,
	KEY_RIGHT_SHIFT,
	KEY_ENTER,
	KEY_SPACE,
	KEY_TAB,
	KEY_CAP,
	KEY_TILDA,
	KEY_F1,
	KEY_F2,
	KEY_F3,
	KEY_F4,
	KEY_F5,
	KEY_F6,
	KEY_F7,
	KEY_F8,
	KEY_F9,
	KEY_F10,
	KEY_F11,
	KEY_F12,
	KEY_BACKSPACE,
	KEY_DELETE,
	KEY_ADD,
	KEY_SUB,
	KEY_MUL,
	KEY_DIV,
	KEY_NUM_0,
	KEY_NUM_1,
	KEY_NUM_2,
	KEY_NUM_3,
	KEY_NUM_4,
	KEY_NUM_5,
	KEY_NUM_6,
	KEY_NUM_7,
	KEY_NUM_8,
	KEY_NUM_9,
};

#undef KEY

class Mouse
{
private:

	float	x;				// Raw X coordinate in pixels
	float	y;				// Raw Y coordinate in pixels
	float	xNormalized;	// Normalized to [0,1] range
	float	yNormalized;	// Normalized to [0,1] range
	float	dx;				// Change in X from last frame
	float	dy;				// Change in Y from last frame
	float	speed;			// Speed multiplier for mouse movement
	bool	leftDown;		// Left is currently down this frame
	bool	leftDownPrev;	// Left was down on the previous frame
	bool	rightDown;		// Right is currently down this frame
	bool	rightDownPrev;	// Right was down on the previous frame
	bool	centered;

public:

	Mouse() : speed( 1.0f ),
		x( 0.0f ),
		y( 0.0f ),
		xNormalized( 0.0f ),
		yNormalized( 0.0f ),
		dx( 0.0f ),
		dy( 0.0f ),
		leftDown( false ),
		leftDownPrev( false ),
		rightDown( false ),
		rightDownPrev( false ),
		centered( false ) {}

	// Raw mouse position in pixels
	inline float X() const				{ return x; }
	inline float Y() const				{ return y; }

	// Mouse position normalized to [0,1] range
	inline float XNormalized() const	{ return xNormalized; }
	inline float YNormalized() const	{ return yNormalized; }

	// Movement delta from this frame from last
	inline float DX() const				{ return dx; }
	inline float DY() const				{ return dy; }

	// Mouse movement speed multiplier
	inline float Speed() const			{ return speed; }

	// Mouse button is currently down
	inline bool IsLeftDown() const		{ return leftDown; }
	inline bool IsRightDown() const		{ return rightDown; }

	// Is the cursor locked in the center of the window and hidden
	inline bool IsCentered() const		{ return centered; }

	// Triggers on release frame
	inline bool LeftClicked() const		{ return ( leftDown && ( leftDownPrev == false ) ); }
	inline bool RightClicked() const	{ return ( rightDown && ( rightDownPrev == false ) ); }

private:

	void NewFrame()
	{
		leftDownPrev = leftDown;
		rightDownPrev = rightDown;
		dx = 0.0f;
		dy = 0.0f;
	}

	inline void SetPosition( const float newX, const float newY )
	{
		x = newX;
		y = newY;
	}

	inline void SetNormalizedPosition( const float newX, const float newY )
	{
		xNormalized = newX;
		yNormalized = newY;
	}

	inline void AccumulateDelta( const float ddx, const float ddy )
	{
		dx += ddx;
		dy += ddy;
	}

	inline void SetSpeed( const float newSpeed )		{ speed = newSpeed;}
	inline void SetLeftDown( const bool down )			{ leftDown = down; }
	inline void SetRightDown( const bool down )			{ rightDown = down; }
	inline void SetCentered( const bool isCentered )	{ centered = isCentered; }

	friend class Input;	// NewFrame()
#ifdef USE_GLFW
	friend void MousePressCallback( GLFWwindow* window, int button, int action, int mods );
	friend void MouseMoveCallback( GLFWwindow* window, double xpos, double ypos );
#endif
};


class Input
{
public:
	Input()
	{
		ClearKeyHistory();
	}

	bool IsKeyPressed( const char key ) {
		return keys[ key ];
	}

	const Mouse& GetMouse() const {
		return mouse;
	}

	void NewFrame()
	{
		mouse.NewFrame();
		memcpy( keys, keys, 255 );
	}

private:
	Mouse	mouse;
	bool	keys[ 256 ];

	void SetKey( const char key, const bool value ) {
		keys[ key ] = value;
	}

	Mouse& GetMouseRef() {
		return mouse;
	}

	void ClearKeyHistory() {
		memset( keys, 0, 255 );
	}

#ifdef USE_GLFW
	friend void KeyCallback( GLFWwindow* window, int key, int scancode, int action, int mods );
	friend void MousePressCallback( GLFWwindow* window, int button, int action, int mods );
	friend void MouseMoveCallback( GLFWwindow* window, double xpos, double ypos );
#endif
};

#ifdef USE_GLFW
void KeyCallback( GLFWwindow* window, int key, int scancode, int action, int mods );
void MousePressCallback( GLFWwindow* window, int button, int action, int mods );
void MouseMoveCallback( GLFWwindow* window, double xpos, double ypos );
#endif