#include "deviceContext.h"
#include <SysCore/log.h>
#include "../render_core/swapChain.h"
#include "../draw_passes/drawpass.h"
#include "../render_resources/gpuImage.h"
#include "../render_core/renderer.h"
#include "../render_binding/bindings.h"
#include "../app/cvar.h"

DeviceContext context;

#ifdef NDEBUG
static bool s_enableValidationLayers = false;
#else
static bool s_enableValidationLayers = true;
static bool s_enableSyncValidationLayers = true;
#endif

MakeCVar( BOOL, r_validation, true );

static const char* const s_validationLayers[] = { "VK_LAYER_KHRONOS_validation" };
static const uint32_t s_validationLayerCount = COUNTARRAY( s_validationLayers );

#ifdef USE_VULKAN
static const char* const s_deviceExtensions[] = {	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
													VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
#ifdef USE_VULKAN_RTX
													VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
													VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
													VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
													VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
													VK_KHR_SPIRV_1_4_EXTENSION_NAME,
													VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
#endif
												};
static const uint32_t s_deviceExtensionCount = COUNTARRAY( s_deviceExtensions );
#endif

static const char* const s_debugExtensions[] = { VK_EXT_DEBUG_MARKER_EXTENSION_NAME };

static const bool s_validateVerbose = false;
static const bool s_validateWarnings = true;
static const bool s_validateErrors = true;

enum class attachedProfiler_t
{
	NONE,
	RENDER_DOC,
	NSIGHT,
};


static attachedProfiler_t DetectProfiler()
{
#if defined( _WIN32 )
	if ( GetModuleHandleA( "renderdoc.dll" ) != nullptr ) {
		return attachedProfiler_t::RENDER_DOC;
	}

	if( ( GetModuleHandleA( "Nvda.Graphics.Interception.dll" ) != nullptr )	||
		( GetModuleHandleA( "nv-nsight-interceptor.dll" ) != nullptr )		||
		( GetModuleHandleA( "nv-nsight-interceptor-win64.dll" ) != nullptr ) )
	{
		return attachedProfiler_t::NSIGHT;
	}
#else
	assert( 0 ); // Not implemented for other platforms yet
#endif
	return attachedProfiler_t::NONE;
}


bool vk_IsDeviceSuitable( VkPhysicalDevice device, VkSurfaceKHR surface, const char* const extensions[], const uint32_t extensionCount )
{
	QueueFamilyIndices indices = vk_FindQueueFamilies( device, surface );

	// Check every required extension is present
	uint32_t availCount = 0;
	vkEnumerateDeviceExtensionProperties( device, nullptr, &availCount, nullptr );
	std::vector<VkExtensionProperties> avail( availCount );
	vkEnumerateDeviceExtensionProperties( device, nullptr, &availCount, avail.data() );

	bool extensionsSupported = true;
	for ( uint32_t i = 0; i < extensionCount; ++i )
	{
		bool found = false;
		for ( const VkExtensionProperties& ext : avail )
		{
			if ( strcmp( extensions[ i ], ext.extensionName ) == 0 )
			{
				found = true;
				break;
			}
		}
		if ( found == false )
		{
			extensionsSupported = false;
			break;
		}
	}

	bool swapChainAdequate = false;
	if ( extensionsSupported )
	{
		swapChainInfo_t swapChainSupport = SwapChain::QuerySwapChainSupport( device, surface );
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	VkPhysicalDeviceFeatures supportedFeatures;
	vkGetPhysicalDeviceFeatures( device, &supportedFeatures );

	return indices.IsComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}


QueueFamilyIndices vk_FindQueueFamilies( VkPhysicalDevice device, VkSurfaceKHR surface )
{
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties( device, &queueFamilyCount, nullptr );

	std::vector<VkQueueFamilyProperties> queueFamilies( queueFamilyCount );
	vkGetPhysicalDeviceQueueFamilyProperties( device, &queueFamilyCount, queueFamilies.data() );

	int i = 0;
	for ( const auto& queueFamily : queueFamilies )
	{
		if ( queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT ) {
			indices.graphicsFamily.set_value( i );
		}

		if ( queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT ) {
			indices.computeFamily.set_value( i );
		}

		VkBool32 presentSupport = false;

		vkGetPhysicalDeviceSurfaceSupportKHR( device, i, surface, &presentSupport );
		if ( presentSupport ) {
			indices.presentFamily.set_value( i );
		}

		if ( indices.IsComplete() ) {
			break;
		}

		i++;
	}

	return indices;
}


VkResult vk_CreateDebugUtilsMessengerEXT( VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger )
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr( instance, "vkCreateDebugUtilsMessengerEXT" );
	if ( func != nullptr ) {
		return func( instance, pCreateInfo, pAllocator, pDebugMessenger );
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}


static VKAPI_ATTR VkBool32 VKAPI_CALL vk_DebugCallback( VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData )
{
	// DXC culls input/outputs and Vulkans warns on unused attributes
	// The shaders are set-up with universal bindings so unused attributes are expected
	// These are really informational warnings so can be suppressed
	static const int32_t suppressedIds[] =
	{
		(int32_t)0x609A13B,		// UNASSIGNED-CoreValidation-Shader-OutputNotConsumed
		(int32_t)0xC81AD50E,	// UNASSIGNED-CoreValidation-Shader-InputNotProduced
	};

	for ( int32_t id : suppressedIds )
	{
		if ( pCallbackData->messageIdNumber == id ) {
			return VK_FALSE;
		}
	}

	const char* system;
	if ( ( messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT ) != 0 ) {
		system = "Vulkan/Validation";
	} else if ( ( messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT ) != 0 ) {
		system = "Vulkan/Performance";
	} else {
		system = "Vulkan";
	}

	logSeverity_t severity;
	if ( ( messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ) != 0 ) {
		severity = logSeverity_t::Error;
	} else if ( ( messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT ) != 0 ) {
		severity = logSeverity_t::Warning;
	} else if ( ( messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT ) != 0 ) {
		severity = logSeverity_t::Info;
	} else {
		severity = logSeverity_t::Verbose;
	}

	LogMsg( system, severity, "%s", pCallbackData->pMessage );

	return VK_FALSE;
}


void vk_PopulateDebugMessengerCreateInfo( VkDebugUtilsMessengerCreateInfoEXT& createInfo )
{
	createInfo = { };
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

	createInfo.messageSeverity = 0;
	if ( s_validateVerbose ) {
		createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
	}

	if ( s_validateWarnings ) {
		createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	}

	if ( s_validateErrors ) {
		createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	}

	VkDebugUtilsMessageTypeFlagsEXT messageFlags = 0;
	messageFlags |= VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
	messageFlags |= VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
	messageFlags |= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

	createInfo.messageType = messageFlags;
	createInfo.pfnUserCallback = vk_DebugCallback;
}


bool vk_CheckValidationLayerSupport()
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties( &layerCount, nullptr );

	std::vector<VkLayerProperties> availableLayers( layerCount );
	vkEnumerateInstanceLayerProperties( &layerCount, availableLayers.data() );

	for ( uint32_t i = 0; i < s_validationLayerCount; ++i )
	{
		const char* layerName = s_validationLayers[ i ];
		bool layerFound = false;

		for ( const VkLayerProperties& layerProperties : availableLayers )
		{
			if ( strcmp( layerName, layerProperties.layerName ) == 0 )
			{
				layerFound = true;
				break;
			}
		}

		if ( layerFound == false ) {
			return false;
		}
	}
	return true;
}



static bool vk_IsExtAvailable( const VkExtensionProperties* exts, uint32_t count, const char* name )
{
	for ( uint32_t i = 0; i < count; ++i ) {
		if ( strcmp( exts[ i ].extensionName, name ) == 0 ) return true;
	}
	return false;
}


void DeviceContext::Create( Window& window )
{
	LOG_SCOPE_SYSTEM( Vulkan );

	// These are declared up here to prevent dangling pointers in Vulkan structs
	const char* requiredExtensions[ 16 ] = {};
	const char* enabledExtensions[ 64 ] = {};
	uint32_t enabledExtensionCount = 0;
	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo queueCreateInfos[ QUEUE_COUNT ] = {};
	uint32_t queueCreateInfoCount = 0;

	enabledExtensionCount = 0;
	for ( uint32_t i = 0; i < s_deviceExtensionCount; ++i ) {
		enabledExtensions[ enabledExtensionCount++ ] = s_deviceExtensions[ i ];
	}
	queueCreateInfoCount = 0;

	// Create Instance
	{
		const bool validationLayersRequested = ( s_enableValidationLayers && r_validation.GetBool() );

		s_enableValidationLayers = ( validationLayersRequested && !m_profilerAttached && vk_CheckValidationLayerSupport() );

		if ( validationLayersRequested && !s_enableValidationLayers ) {
			LogMsg( logSeverity_t::Warning, "Validation layers requested but unavailable." );
		}

		VkApplicationInfo appInfo{ };
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Xtensa";
		appInfo.applicationVersion = VK_MAKE_VERSION( 1, 0, 0 );
		appInfo.pEngineName = "Xtensa";
		appInfo.engineVersion = VK_MAKE_VERSION( 1, 0, 0 );
		appInfo.apiVersion = VK_API_VERSION_1_3;

		VkInstanceCreateInfo createInfo{ };
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		uint32_t instanceExtPropCount = 0;
		vkEnumerateInstanceExtensionProperties( nullptr, &instanceExtPropCount, nullptr );
		VkExtensionProperties extensionProperties[ 256 ];
		instanceExtPropCount = instanceExtPropCount < COUNTARRAY( extensionProperties ) ? instanceExtPropCount : COUNTARRAY( extensionProperties );
		vkEnumerateInstanceExtensionProperties( nullptr, &instanceExtPropCount, extensionProperties );

		LogMsg( logSeverity_t::Verbose, "Available extensions:" );
		for ( uint32_t i = 0; i < instanceExtPropCount; ++i ) {
			LogMsg( logSeverity_t::Verbose, "\t%s", extensionProperties[ i ].extensionName );
		}

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		VkValidationFeaturesEXT validationFeatures{};
		if ( s_enableValidationLayers )
		{
			createInfo.enabledLayerCount = s_validationLayerCount;
			createInfo.ppEnabledLayerNames = s_validationLayers;

			if( s_enableSyncValidationLayers && !m_profilerAttached )
			{
				static const VkValidationFeatureEnableEXT validationEnables[] = {
					VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
				};
				validationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
				validationFeatures.enabledValidationFeatureCount = COUNTARRAY( validationEnables );
				validationFeatures.pEnabledValidationFeatures = validationEnables;

				vk_PopulateDebugMessengerCreateInfo( debugCreateInfo );

				validationFeatures.pNext = &debugCreateInfo;
				createInfo.pNext = &validationFeatures;
			}
		}
		else
		{
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		uint32_t requiredExtensionCount = 0;
#ifdef USE_GLFW
		{
			uint32_t glfwCount = 0;
			const char** glfwExtensions = glfwGetRequiredInstanceExtensions( &glfwCount );
			for ( uint32_t i = 0; i < glfwCount && requiredExtensionCount < COUNTARRAY( requiredExtensions ); ++i ) {
				requiredExtensions[ requiredExtensionCount++ ] = glfwExtensions[ i ];
			}
		}
#endif
		if ( s_enableValidationLayers )
		{
			assert( requiredExtensionCount + 2 <= COUNTARRAY( requiredExtensions ) );
			requiredExtensions[ requiredExtensionCount++ ] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
			requiredExtensions[ requiredExtensionCount++ ] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
		}

		createInfo.enabledExtensionCount = requiredExtensionCount;
		createInfo.ppEnabledExtensionNames = requiredExtensions;

		VK_CHECK_RESULT( vkCreateInstance( &createInfo, nullptr, &instance ) );

		vk_SetObjectName( (uint64_t)instance, VK_OBJECT_TYPE_INSTANCE, "VulkanInstance" );
	}

	// Debug Messenger
	if ( s_enableValidationLayers )
	{
		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		vk_PopulateDebugMessengerCreateInfo( createInfo );

		VK_CHECK_RESULT( vk_CreateDebugUtilsMessengerEXT( instance, &createInfo, nullptr, &debugMessenger ) );
	}

	// Window Surface
	{
#ifdef USE_GLFW
		window.CreateGlfwSurface( context.instance );
#endif
	}

	// Pick physical device
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices( instance, &deviceCount, nullptr );

		if ( deviceCount == 0 ) {
			THROW_ERROR( "Failed to find GPUs with Vulkan support!" );
		}

		VkPhysicalDevice devices[ 16 ];
		deviceCount = deviceCount < COUNTARRAY( devices ) ? deviceCount : COUNTARRAY( devices );
		vkEnumeratePhysicalDevices( instance, &deviceCount, devices );

		deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

		for ( uint32_t i = 0; i < deviceCount; ++i )
		{
			const VkPhysicalDevice device = devices[ i ];
			if ( vk_IsDeviceSuitable( device, window.vk_surface, s_deviceExtensions, s_deviceExtensionCount ) )
			{
				vkGetPhysicalDeviceProperties( device, &deviceProperties );
				vkGetPhysicalDeviceFeatures2( device, &deviceFeatures );
				physicalDevice = device;
				break;
			}
		}

		if ( physicalDevice == VK_NULL_HANDLE ) {
			THROW_ERROR( "Failed to find a suitable GPU!" );
		}
		vk_SetObjectName( (uint64_t)physicalDevice, VK_OBJECT_TYPE_DEVICE, "VulkanPhysicalDevice" );
	}

	// Create logical device
	{
		QueueFamilyIndices indices = vk_FindQueueFamilies( physicalDevice, window.vk_surface );
		queueFamilyIndices[ QUEUE_GRAPHICS ] = indices.graphicsFamily.value();
		queueFamilyIndices[ QUEUE_PRESENT ] = indices.presentFamily.value();
		queueFamilyIndices[ QUEUE_COMPUTE ] = indices.computeFamily.value();

		const uint32_t candidateFamilies[] = {
			indices.graphicsFamily.value(),
			indices.presentFamily.value(),
			indices.computeFamily.value()
		};

		uint32_t uniqueFamilies[ QUEUE_COUNT ];
		uint32_t uniqueFamilyCount = 0;
		for ( uint32_t i = 0; i < COUNTARRAY( candidateFamilies ); ++i )
		{
			bool duplicate = false;
			for ( uint32_t j = 0; j < uniqueFamilyCount; ++j )
			{
				if ( uniqueFamilies[ j ] == candidateFamilies[ i ] )
				{
					duplicate = true;
					break;
				}
			}
			if ( !duplicate ) {
				uniqueFamilies[ uniqueFamilyCount++ ] = candidateFamilies[ i ];
			}
		}

		assert( uniqueFamilyCount >= 1 );

		for ( uint32_t i = 0; i < uniqueFamilyCount; ++i )
		{
			queueCreateInfos[ i ] = {};
			queueCreateInfos[ i ].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfos[ i ].queueFamilyIndex = uniqueFamilies[ i ];
			queueCreateInfos[ i ].queueCount = 1;
			queueCreateInfos[ i ].pQueuePriorities = &queuePriority;
		}
		queueCreateInfoCount = uniqueFamilyCount;

		VkDeviceCreateInfo createInfo{ };
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = queueCreateInfoCount;
		createInfo.pQueueCreateInfos = queueCreateInfos;

		deviceFeatures.features.samplerAnisotropy = VK_TRUE;
		deviceFeatures.features.fillModeNonSolid = VK_TRUE;
		deviceFeatures.features.sampleRateShading = VK_TRUE;
		deviceFeatures.features.pipelineStatisticsQuery = VK_TRUE;
		deviceFeatures.features.vertexPipelineStoresAndAtomics = VK_TRUE;
		deviceFeatures.features.fragmentStoresAndAtomics = VK_TRUE;
#ifdef USE_VULKAN_RTX
		deviceFeatures.features.shaderStorageImageWriteWithoutFormat = VK_TRUE;

		enabledRayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
		enabledRayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;

		enabledAccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
		enabledAccelerationStructureFeatures.accelerationStructure = VK_TRUE;
		enabledAccelerationStructureFeatures.pNext = &enabledRayTracingPipelineFeatures;

		deviceFeatures.pNext = &accelerationStructureFeatures;
		vkGetPhysicalDeviceFeatures2( physicalDevice, &deviceFeatures );
#endif

		createInfo.pEnabledFeatures = &deviceFeatures.features;

		// Enumerate all available device extensions once; used for optional extension checks below
		uint32_t availDevExtCount = 0;
		vkEnumerateDeviceExtensionProperties( physicalDevice, nullptr, &availDevExtCount, nullptr );

		VkExtensionProperties availDevExts[ 512 ];
		availDevExtCount = availDevExtCount < COUNTARRAY( availDevExts ) ? availDevExtCount : COUNTARRAY( availDevExts );
		vkEnumerateDeviceExtensionProperties( physicalDevice, nullptr, &availDevExtCount, availDevExts );

		for ( uint32_t i = 0; i < COUNTARRAY( s_debugExtensions ); ++i )
		{
			if ( vk_IsExtAvailable( availDevExts, availDevExtCount, s_debugExtensions[ i ] ) ) {
				enabledExtensions[ enabledExtensionCount++ ] = s_debugExtensions[ i ];
			}
		}

		createInfo.enabledExtensionCount = enabledExtensionCount;
		createInfo.ppEnabledExtensionNames = enabledExtensions;
		if ( s_enableValidationLayers )
		{
			createInfo.enabledLayerCount = s_validationLayerCount;
			createInfo.ppEnabledLayerNames = s_validationLayers;
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeature = {};
		dynamicRenderingFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
		dynamicRenderingFeature.dynamicRendering = VK_TRUE;
		dynamicRenderingFeature.pNext = nullptr;

		VkPhysicalDeviceVulkan12Features vk12Features = {};
		vk12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		vk12Features.vulkanMemoryModel = VK_TRUE;
		vk12Features.vulkanMemoryModelDeviceScope = VK_TRUE;
		vk12Features.runtimeDescriptorArray = VK_TRUE;
		vk12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
#ifdef USE_VULKAN_RTX
		vk12Features.bufferDeviceAddress = VK_TRUE;
#endif
		vk12Features.pNext = &dynamicRenderingFeature;

#ifdef USE_VULKAN_RTX
		enabledRayTracingPipelineFeatures.pNext = &vk12Features;
		createInfo.pNext = &enabledAccelerationStructureFeatures;
#else
		createInfo.pNext = &vk12Features;
#endif

		VK_CHECK_RESULT( vkCreateDevice( physicalDevice, &createInfo, nullptr, &device ) );
		vk_SetObjectName( (uint64_t)device, VK_OBJECT_TYPE_DEVICE, "VulkanLogicalDevice" );

		vkGetDeviceQueue( device, indices.graphicsFamily.value(), 0, &gfxContext );
		vkGetDeviceQueue( device, indices.presentFamily.value(), 0, &presentQueue );
		vkGetDeviceQueue( device, indices.computeFamily.value(), 0, &computeContext );
	}

	// Debug Markers
	{
		m_debugMarkersEnabled = false;
		for ( uint32_t i = 0; i < enabledExtensionCount; ++i )
		{
			if ( strcmp( enabledExtensions[ i ], VK_EXT_DEBUG_MARKER_EXTENSION_NAME ) == 0 )
			{
				m_debugMarkersEnabled = true;
				break;
			}
		}

#ifdef USE_VULKAN_RTX
		// Ray tracing functions and properties
		{
			rayTracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
			VkPhysicalDeviceProperties2 deviceProperties2{};
			deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
			deviceProperties2.pNext = &rayTracingPipelineProperties;
			vkGetPhysicalDeviceProperties2( physicalDevice, &deviceProperties2 );

			vkGetBufferDeviceAddressKHR = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>( vkGetDeviceProcAddr( device, "vkGetBufferDeviceAddressKHR" ) );
			vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>( vkGetDeviceProcAddr( device, "vkCmdBuildAccelerationStructuresKHR" ) );
			vkBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkBuildAccelerationStructuresKHR>( vkGetDeviceProcAddr( device, "vkBuildAccelerationStructuresKHR" ) );
			vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>( vkGetDeviceProcAddr( device, "vkCreateAccelerationStructureKHR" ) );
			vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>( vkGetDeviceProcAddr( device, "vkDestroyAccelerationStructureKHR" ) );
			vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>( vkGetDeviceProcAddr( device, "vkGetAccelerationStructureBuildSizesKHR" ) );
			vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>( vkGetDeviceProcAddr( device, "vkGetAccelerationStructureDeviceAddressKHR" ) );
			vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>( vkGetDeviceProcAddr( device, "vkCmdTraceRaysKHR" ) );
			vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>( vkGetDeviceProcAddr( device, "vkGetRayTracingShaderGroupHandlesKHR" ) );
			vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>( vkGetDeviceProcAddr( device, "vkCreateRayTracingPipelinesKHR" ) );
		}
#endif

		if ( m_debugMarkersEnabled )
		{
			LogMsg( "Enabling debug markers." );

			fnDebugMarkerSetObjectTag = (PFN_vkDebugMarkerSetObjectTagEXT)vkGetDeviceProcAddr( device, "vkDebugMarkerSetObjectTagEXT" );
			fnDebugMarkerSetObjectName = (PFN_vkDebugMarkerSetObjectNameEXT)vkGetDeviceProcAddr( device, "vkDebugMarkerSetObjectNameEXT" );
			fnCmdDebugMarkerBegin = (PFN_vkCmdDebugMarkerBeginEXT)vkGetDeviceProcAddr( device, "vkCmdDebugMarkerBeginEXT" );
			fnCmdDebugMarkerEnd = (PFN_vkCmdDebugMarkerEndEXT)vkGetDeviceProcAddr( device, "vkCmdDebugMarkerEndEXT" );
			fnCmdDebugMarkerInsert = (PFN_vkCmdDebugMarkerInsertEXT)vkGetDeviceProcAddr( device, "vkCmdDebugMarkerInsertEXT" );

			debugMarkersEnabled = true;
			debugMarkersEnabled = debugMarkersEnabled && ( fnDebugMarkerSetObjectTag != VK_NULL_HANDLE );
			debugMarkersEnabled = debugMarkersEnabled && ( fnDebugMarkerSetObjectName != VK_NULL_HANDLE );
			debugMarkersEnabled = debugMarkersEnabled && ( fnCmdDebugMarkerBegin != VK_NULL_HANDLE );
			debugMarkersEnabled = debugMarkersEnabled && ( fnCmdDebugMarkerEnd != VK_NULL_HANDLE );
			debugMarkersEnabled = debugMarkersEnabled && ( fnCmdDebugMarkerInsert != VK_NULL_HANDLE );
		}
		else {
			LogMsg( "Debug markers \"%s\" disabled.", VK_EXT_DEBUG_MARKER_EXTENSION_NAME );
		}
	}

	// Debug
	{
		fnCmdSetDebugUtilsObjectName = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr( device, "vkSetDebugUtilsObjectNameEXT" );
	}

	// Descriptor Pool
	{
#ifdef USE_VULKAN_RTX
		const uint32_t subPoolCount = 7;
#else
		const uint32_t subPoolCount = 6;
#endif

		VkDescriptorPoolSize poolSizes[ subPoolCount ];
		poolSizes[ 0 ].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[ 0 ].descriptorCount = DescriptorPoolMaxUniformBuffers;
		poolSizes[ 1 ].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		poolSizes[ 1 ].descriptorCount = DescriptorPoolMaxStorageBuffers;
		poolSizes[ 2 ].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[ 2 ].descriptorCount = DescriptorPoolMaxComboImages;
		poolSizes[ 3 ].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		poolSizes[ 3 ].descriptorCount = DescriptorPoolMaxImages;
		poolSizes[ 4 ].type = VK_DESCRIPTOR_TYPE_SAMPLER;
		poolSizes[ 4 ].descriptorCount = DescriptorPoolMaxSamplers;
		poolSizes[ 5 ].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		poolSizes[ 5 ].descriptorCount = DescriptorPoolMaxStorageImages;
#ifdef USE_VULKAN_RTX
		poolSizes[ 6 ].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		poolSizes[ 6 ].descriptorCount = DescriptorPoolMaxAccelStructures;
#endif

		VkDescriptorPoolCreateInfo poolInfo{ };
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = subPoolCount;
		poolInfo.pPoolSizes = poolSizes;
		poolInfo.maxSets = DescriptorPoolMaxSets;
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

		VK_CHECK_RESULT( vkCreateDescriptorPool( device, &poolInfo, nullptr, &descriptorPool ) );

		vk_SetObjectName( (uint64_t)descriptorPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "DescriptorPool (Uniform | Storage | ImageSampler | Image)" );
	}

	m_profilerAttached = ( DetectProfiler() != attachedProfiler_t::NONE );

	bufferId = 0;
}


void DeviceContext::Destroy( Window& window )
{
	vkDestroyQueryPool( device, statQueryPool, nullptr );
	vkDestroyQueryPool( device, timestampQueryPool, nullptr );
	vkDestroyQueryPool( device, occlusionQueryPool, nullptr );

	vkDestroyDescriptorPool( device, descriptorPool, nullptr );

	vkDestroyDevice( device, nullptr );

	if ( s_enableValidationLayers )
	{
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr( instance, "vkDestroyDebugUtilsMessengerEXT" );
		if ( func != nullptr ) {
			func( instance, debugMessenger, nullptr );
		}
	}

#ifdef USE_GLFW
	window.DestroyGlfwSurface( context.instance );
#endif
	
	vkDestroyInstance( instance, nullptr );
}
