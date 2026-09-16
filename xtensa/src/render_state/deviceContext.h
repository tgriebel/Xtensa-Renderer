#pragma once
#include "../globals/common.h"
#include "cmdContext.h"

class Window;
class ImageView;
class FrameBuffer;

#ifdef USE_VULKAN
struct swapChainInfo_t
{
	VkSurfaceCapabilitiesKHR		capabilities;
	std::vector<VkSurfaceFormatKHR>	formats;
	std::vector<VkPresentModeKHR>	presentModes;
};
#endif


struct copyImageParms_t
{
	int32_t					x;
	int32_t					y;
	int32_t					z;
	int32_t					width;
	int32_t					height;
	int32_t					depth;

	uint32_t baseMip;
	uint32_t mipLevels;
	uint32_t baseArray;
	uint32_t arrayCount;
};


enum class resolveMode_t : uint32_t
{
	AVERAGE     = 0,	// Color — weighted average across all samples
	SAMPLE_ZERO = 1,	// Normals, IDs — take sample 0 directly (no blurring at edges)
	MIN         = 2,	// Depth — conservative minimum
	MAX         = 3,	// Depth — maximum
};


struct resolveImageInfo_t
{
	Image*			src;
	Image*			dst;
	resolveMode_t	mode;
	uint32_t		baseMip;
	uint32_t		baseArray;
	uint32_t		arrayCount;
	bool			transitionSourceFromWrite;
	bool			writeSourceAfterResolve;
};


class DeviceContext
{
private:
	bool m_profilerAttached = false;
	bool m_debugMarkersEnabled = false;
	bool m_validationEnabled = false;

public:
#ifdef USE_VULKAN
	VkDevice											device;
	VkPhysicalDevice									physicalDevice;
	VkInstance											instance;
	VkPhysicalDeviceProperties							deviceProperties;
	VkPhysicalDeviceFeatures2							deviceFeatures;
	VkDebugUtilsMessengerEXT							debugMessenger;
	VkDescriptorPool									descriptorPool;
	VkQueryPool											statQueryPool;
	VkQueryPool											timestampQueryPool;
	VkQueryPool											occlusionQueryPool;
	VkQueue												gfxContext;
	VkQueue												presentQueue;
	VkQueue												computeContext;
	uint32_t											queueFamilyIndices[ QUEUE_COUNT ];

	bool												debugMarkersEnabled = false;
	PFN_vkDebugMarkerSetObjectTagEXT					fnDebugMarkerSetObjectTag = VK_NULL_HANDLE;
	PFN_vkDebugMarkerSetObjectNameEXT					fnDebugMarkerSetObjectName = VK_NULL_HANDLE;
	PFN_vkCmdDebugMarkerBeginEXT						fnCmdDebugMarkerBegin = VK_NULL_HANDLE;
	PFN_vkCmdDebugMarkerEndEXT							fnCmdDebugMarkerEnd = VK_NULL_HANDLE;
	PFN_vkCmdDebugMarkerInsertEXT						fnCmdDebugMarkerInsert = VK_NULL_HANDLE;
	PFN_vkSetDebugUtilsObjectNameEXT					fnCmdSetDebugUtilsObjectName = VK_NULL_HANDLE;
#ifdef USE_VULKAN_RTX
	PFN_vkGetBufferDeviceAddressKHR						vkGetBufferDeviceAddressKHR = VK_NULL_HANDLE;
	PFN_vkCreateAccelerationStructureKHR				vkCreateAccelerationStructureKHR = VK_NULL_HANDLE;
	PFN_vkDestroyAccelerationStructureKHR				vkDestroyAccelerationStructureKHR = VK_NULL_HANDLE;
	PFN_vkGetAccelerationStructureBuildSizesKHR			vkGetAccelerationStructureBuildSizesKHR = VK_NULL_HANDLE;
	PFN_vkGetAccelerationStructureDeviceAddressKHR		vkGetAccelerationStructureDeviceAddressKHR = VK_NULL_HANDLE;
	PFN_vkCmdBuildAccelerationStructuresKHR				vkCmdBuildAccelerationStructuresKHR = VK_NULL_HANDLE;
	PFN_vkBuildAccelerationStructuresKHR				vkBuildAccelerationStructuresKHR = VK_NULL_HANDLE;
	PFN_vkCmdTraceRaysKHR								vkCmdTraceRaysKHR = VK_NULL_HANDLE;
	PFN_vkGetRayTracingShaderGroupHandlesKHR			vkGetRayTracingShaderGroupHandlesKHR = VK_NULL_HANDLE;
	PFN_vkCreateRayTracingPipelinesKHR					vkCreateRayTracingPipelinesKHR = VK_NULL_HANDLE;

	VkPhysicalDeviceRayTracingPipelinePropertiesKHR		rayTracingPipelineProperties{};
	VkPhysicalDeviceAccelerationStructureFeaturesKHR	accelerationStructureFeatures{};

	VkPhysicalDeviceBufferDeviceAddressFeatures			enabledBufferDeviceAddresFeatures{};
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR		enabledRayTracingPipelineFeatures{};
	VkPhysicalDeviceAccelerationStructureFeaturesKHR	enabledAccelerationStructureFeatures{};
#endif
#endif
	// "bufferId" flips between double/triple buffers - 0, 1, 2
	uint32_t							bufferId;

	void	Create( Window& window );
	void	Destroy( Window& window );

	inline bool IsProfilerAttached() const { return m_profilerAttached; }
};

extern DeviceContext context;

#ifdef USE_VULKAN
#define VK_CHECK_RESULT( f )																			\
{																										\
	VkResult res = (f);																					\
	if (res != VK_SUCCESS)																				\
	{																									\
		THROW_ERROR( #f );																				\
		assert(res == VK_SUCCESS);																		\
	}																									\
}
#endif

struct imageInfo_t;
struct imageSubResourceView_t;
struct scissor_t;
union renderPassTransition_t;
class GpuImage;
class AllocatorMemory;
class DrawPass;

enum imageSamples_t : uint8_t;

#ifdef USE_VULKAN
bool				vk_IsDeviceSuitable( VkPhysicalDevice device, VkSurfaceKHR surface, const char* const extensions[], uint32_t extensionCount );
QueueFamilyIndices	vk_FindQueueFamilies( VkPhysicalDevice device, VkSurfaceKHR surface );
bool				vk_ValidTextureFormat( const VkFormat format, VkImageTiling tiling, VkFormatFeatureFlags features );
uint32_t			vk_FindMemoryType( uint32_t typeFilter, VkMemoryPropertyFlags properties );
int32_t				vk_MapToGlslCubemapConvention( const uint32_t index );
VkImageView			vk_CreateImageView( const VkImage image, const imageInfo_t& info, const char* debugName = "", const uint32_t debugBufferId = 0 );
VkImageView			vk_CreateImageView( const VkImage image, const imageInfo_t& info, const imageSubResourceView_t& subResourceView, const char* debugName = "", const uint32_t debugBufferId = 0 );
void				vk_TransitionImageLayout( VkCommandBuffer cmdBuffer, const Image* image, const imageSubResourceView_t& subView, swapBuffering_t buffering, gpuImageStateFlags_t current, gpuImageStateFlags_t next );
void				vk_TransitionImageLayout( VkCommandBuffer cmdBuffer, const GpuImage* gpuImage, const imageSubResourceView_t& subView, swapBuffering_t buffering, gpuImageStateFlags_t current, gpuImageStateFlags_t next );
void				vk_GenerateMipmaps( VkCommandBuffer cmdBuffer, Image* image );
void				vk_QuadDraw( CommandList& cmdContext, const hdl_t pipeLineHandle, const vec4f& extent, const scissor_t& clipRect, const DrawPass* pass );
void				vk_RenderImageShader( CommandList& cmdContext, const hdl_t pipeLineHandle, const DrawPass* pass, const renderPassTransition_t& transitionState );
void				vk_CopyImage( VkCommandBuffer cmdBuffer, const Image* src, const copyImageParms_t& srcParms, Image* dst, const copyImageParms_t& dstParms );
void				vk_CopyImage( VkCommandBuffer cmdBuffer, const Image& src, Image& dst );
void				vk_CopyImage( VkCommandBuffer cmdBuffer, const ImageView& src, ImageView& dst );
void				vk_ResolveImage( VkCommandBuffer cmdBuffer, const resolveImageInfo_t& info );
void				vk_UploadImageData( VkCommandBuffer cmdBuffer, Image* texture, const copyImageParms_t& copyParms, GpuBuffer& buffer );
imageSamples_t		vk_MaxImageSamples();
VkShaderModule		vk_CreateShaderModule( const std::vector<char>& code, const char* debugName );
VkResult			vk_CreateDebugUtilsMessengerEXT( VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger );
std::string			vk_BuildObjectName( const char* typeName, const char* baseName, int32_t bufferId = -1 );
void				vk_SetObjectName( const uint64_t handle, VkObjectType objectType, const char* name );
void				vk_MarkerSetObjectTag( uint64_t object, VkDebugReportObjectTypeEXT objectType, uint64_t name, size_t tagSize, const void* tag );
void				vk_MarkerSetObjectName( uint64_t object, VkDebugReportObjectTypeEXT objectType, const char* name );
#endif