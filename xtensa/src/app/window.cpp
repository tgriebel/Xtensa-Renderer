#include "window.h"
#include "../render_state/deviceContext.h"

#include "../../../external/tinyfiledialogs/tinyfiledialogs.h"

#if defined( USE_IMGUI )
#include "../../../external/imgui/imgui.h"
#include "../../../external/imgui/backends/imgui_impl_glfw.h"
#endif

#include "cvar.h"

extern CVar r_fullscreenMode;
extern CVar r_windowWidth;
extern CVar r_windowHeight;

#define KEY_MAP( k ) { GLFW_KEY_##k, KEY_##k }

struct KeyPair
{
	int		glfwKey;
	key_t	key;
};

static KeyPair GlfwKeyMap[] =
{
	KEY_MAP( 0 ),
	KEY_MAP( 1 ),
	KEY_MAP( 2 ),
	KEY_MAP( 3 ),
	KEY_MAP( 4 ),
	KEY_MAP( 5 ),
	KEY_MAP( 6 ),
	KEY_MAP( 7 ),
	KEY_MAP( 8 ),
	KEY_MAP( 9 ),
	KEY_MAP( A ),
	KEY_MAP( B ),
	KEY_MAP( C ),
	KEY_MAP( D ),
	KEY_MAP( E ),
	KEY_MAP( F ),
	KEY_MAP( G ),
	KEY_MAP( H ),
	KEY_MAP( I ),
	KEY_MAP( J ),
	KEY_MAP( K ),
	KEY_MAP( L ),
	KEY_MAP( M ),
	KEY_MAP( N ),
	KEY_MAP( O ),
	KEY_MAP( P ),
	KEY_MAP( Q ),
	KEY_MAP( R ),
	KEY_MAP( S ),
	KEY_MAP( T ),
	KEY_MAP( U ),
	KEY_MAP( V ),
	KEY_MAP( W ),
	KEY_MAP( X ),
	KEY_MAP( Y ),
	KEY_MAP( Z ),

#ifdef USE_GLFW
	{ GLFW_KEY_KP_0,		KEY_NUM_0 },
	{ GLFW_KEY_KP_1,		KEY_NUM_1 },
	{ GLFW_KEY_KP_2,		KEY_NUM_2 },
	{ GLFW_KEY_KP_3,		KEY_NUM_3 },
	{ GLFW_KEY_KP_4,		KEY_NUM_4 },
	{ GLFW_KEY_KP_5,		KEY_NUM_5 },
	{ GLFW_KEY_KP_6,		KEY_NUM_6 },
	{ GLFW_KEY_KP_7,		KEY_NUM_7 },
	{ GLFW_KEY_KP_8,		KEY_NUM_8 },
	{ GLFW_KEY_KP_9,		KEY_NUM_9 },
	{ GLFW_KEY_KP_ADD,		KEY_ADD },
	{ GLFW_KEY_KP_SUBTRACT,	KEY_SUB},
	{ GLFW_KEY_KP_MULTIPLY,	KEY_MUL },
	{ GLFW_KEY_KP_DIVIDE,	KEY_DIV },

	{ GLFW_KEY_LEFT_ALT,	KEY_LEFT_ALT },
	{ GLFW_KEY_RIGHT_ALT,	KEY_RIGHT_ALT },
	{ GLFW_KEY_LEFT,		KEY_LEFT_ARROW },
	{ GLFW_KEY_RIGHT,		KEY_RIGHT_ARROW },
	{ GLFW_KEY_UP,			KEY_UP_ARROW },
	{ GLFW_KEY_DOWN,		KEY_DOWN_ARROW },
#endif
};

#undef KEY_MAP

#ifdef USE_GLFW
void FramebufferResizeCallback( GLFWwindow* window, int width, int height )
{
	Window* app = reinterpret_cast< Window* >( glfwGetWindowUserPointer( window ) );
	app->RequestImageResize();
}


void KeyCallback( GLFWwindow* window, int key, int scancode, int action, int mods )
{
	Window* app = reinterpret_cast< Window* >( glfwGetWindowUserPointer( window ) );

	const uint32_t mappedKeys = COUNTARRAY( GlfwKeyMap );

	for( uint32_t k = 0; k < mappedKeys; ++k )
	{
		if( key == GlfwKeyMap[k].glfwKey ) {
			app->input.SetKey( GlfwKeyMap[ k ].key, ( action != GLFW_RELEASE ) );
			break;
		}
	}

	if ( key == GLFW_KEY_LEFT_ALT ) {
		if ( action != GLFW_RELEASE ) {
			app->mouseLocked = !app->mouseLocked;
		}
	}
}


void MousePressCallback( GLFWwindow* window, int button, int action, int mods )
{
	Window* app = reinterpret_cast< Window* >( glfwGetWindowUserPointer( window ) );
	Mouse& mouse = app->input.GetMouseRef();

	if ( button == GLFW_MOUSE_BUTTON_LEFT ) {
		mouse.SetLeftDown( action != GLFW_RELEASE );
	}

	if ( button == GLFW_MOUSE_BUTTON_RIGHT ) {
		mouse.SetRightDown( action != GLFW_RELEASE );
	}
}


void MouseMoveCallback( GLFWwindow* window, double xpos, double ypos )
{
	Window* appWindow = reinterpret_cast< Window* >( glfwGetWindowUserPointer( window ) );
	Mouse& mouse = appWindow->input.GetMouseRef();

	static double lastX = 0.0;
	static double lastY = 0.0;
	static bool firstMove = true;

	mouse.SetPosition( static_cast<float>( xpos ), static_cast<float>( ypos ) );

	const vec2f ndc = appWindow->GetNdc( mouse.X(), mouse.Y() );
	mouse.SetNormalizedPosition( ndc.x, ndc.y );

	if ( firstMove )
	{
		lastX = xpos;
		lastY = ypos;
		firstMove = false;
		return;
	}

	mouse.AccumulateDelta( static_cast<float>( xpos - lastX ), static_cast<float>( ypos - lastY ) );

	lastX = xpos;
	lastY = ypos;

	if ( appWindow->IsMouseLocked() == false )
	{
		firstMove = true;
		mouse.SetCentered( false );
		glfwSetInputMode( window, GLFW_CURSOR, GLFW_CURSOR_NORMAL );
		return;
	}

	glfwSetInputMode( window, GLFW_CURSOR, GLFW_CURSOR_DISABLED );
	mouse.SetCentered( true );
}
#endif


void Window::BeginFrame()
{
	input.NewFrame();

#if defined( USE_IMGUI ) && defined( USE_GLFW )
	ImGui_ImplGlfw_NewFrame();
#endif
}


void Window::EndFrame()
{

}


bool Window::IsOpen() const
{
#ifdef USE_GLFW
	return ( glfwWindowShouldClose( window ) == false );
#endif
}


bool Window::IsMouseLocked() const
{
	return mouseLocked;
}


std::string Window::OpenFileDialog( const std::string& title, const std::vector<const char*>& filters, const std::string& filterDesc )
{
#ifdef USE_TINYFD
	tinyfd_assumeGraphicDisplay = 1;

	char const* fileFilter[1] = { "*.obj" };

	char* fileName = tinyfd_openFileDialog(
		title.c_str(),
		"",
		static_cast<int>(filters.size()),
		filters.data(),
		filterDesc.c_str(),
		0 );

	return ( fileName == nullptr ) ? "" : fileName;
#else
	return "";
#endif
}


void Window::PumpMessages()
{
#ifdef USE_GLFW
	glfwPollEvents();
#endif
}


void Window::Init()
{
#ifdef USE_GLFW
	glfwInit();

	glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
	glfwWindowHint( GLFW_RESIZABLE, GLFW_TRUE );

	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode( monitor );

	const fullScreenMode_t fullscreenMode = static_cast<fullScreenMode_t>( r_fullscreenMode.GetInt() );

	isFullscreen = ( fullscreenMode == fullScreenMode_t::FULLSCREEN_BORDERLESS ) || ( fullscreenMode == fullScreenMode_t::FULLSCREEN_EXCLUSIVE );
	isBorderless = ( fullscreenMode == fullScreenMode_t::WINDOW_BORDERLESS ) || ( fullscreenMode == fullScreenMode_t::FULLSCREEN_BORDERLESS );

	if( isBorderless )
	{		
		glfwWindowHint( GLFW_DECORATED, GLFW_FALSE );
		glfwWindowHint( GLFW_RED_BITS, mode->redBits );
		glfwWindowHint( GLFW_GREEN_BITS, mode->greenBits );
		glfwWindowHint( GLFW_BLUE_BITS, mode->blueBits );
		glfwWindowHint( GLFW_REFRESH_RATE, mode->refreshRate );
	}

	
	if( isFullscreen )
	{
		if( isBorderless )
		{
			window = glfwCreateWindow( mode->width, mode->height, ApplicationName, monitor, nullptr );
			glfwSetWindowPos( window, 0, 0 );
		}
		else
		{
			window = glfwCreateWindow( mode->width, mode->height, ApplicationName, nullptr, nullptr );
		}
	}
	else
	{
		const int32_t windowWidth = ( r_windowWidth.GetInt() > 0 ) ? r_windowWidth.GetInt() : DefaultDisplayWidth;
		const int32_t windowHeight = ( r_windowHeight.GetInt() > 0 ) ? r_windowHeight.GetInt() : DefaultDisplayHeight;

		window = glfwCreateWindow( windowWidth, windowHeight, ApplicationName, nullptr, nullptr );
	}

	glfwSetWindowUserPointer( window, this );
	glfwSetInputMode( window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN );
	glfwSetFramebufferSizeCallback( window, FramebufferResizeCallback );
	glfwSetKeyCallback( window, KeyCallback );
	glfwSetMouseButtonCallback( window, MousePressCallback );
	glfwSetCursorPosCallback( window, MouseMoveCallback );
#endif

	mouseLocked = true;
}


vec2f Window::GetNdc( const float x, const float y )
{
	const vec2f screenPoint = vec2f( x, y );
	int w, h;
	GetWindowSize( w, h );
	const vec2f ndc = 2.0f * Multiply( screenPoint, vec2f( 1.0f / w, 1.0f / h ) ) - vec2f( 1.0f );
	return ndc;
}


void Window::GetWindowPosition( int& x, int& y )
{
#ifdef USE_GLFW
	glfwGetWindowPos( window, &x, &y );
#endif
}


void Window::GetWindowSize( int& width, int& height )
{
#ifdef USE_GLFW
	glfwGetWindowSize( window, &width, &height );
#endif
}


void Window::QueryWindowFrameBufferSize( int& width, int& height, const bool wait )
{
#ifdef USE_GLFW
	glfwGetFramebufferSize( window, &width, &height );
	if( wait )
	{
		while ( width == 0 || height == 0 )
		{
			glfwGetFramebufferSize( window, &width, &height );
			glfwWaitEvents();
		}
	}
#endif
}


float Window::QueryWindowFrameBufferAspect( const bool wait )
{
	int width, height;
	QueryWindowFrameBufferSize( width, height, wait );
	return width / static_cast<float>( height );
}

#if defined( USE_VULKAN ) && defined( USE_GLFW)
void Window::CreateGlfwSurface( const VkInstance instance )
{
	VK_CHECK_RESULT( glfwCreateWindowSurface( instance, window, nullptr, &vk_surface ) );
}


void Window::DestroyGlfwSurface( const VkInstance instance )
{
	vkDestroySurfaceKHR( context.instance, vk_surface, nullptr );
}
#endif
