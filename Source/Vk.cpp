#include "Vk.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

_Vk Vk;

static const VkDeviceSize UPLOAD_BUFFER_SIZE = 1024 * 1024 * 1024;
static const VkDeviceSize UPLOAD_BUFFER_MASK = UPLOAD_BUFFER_SIZE - 1;
static_assert((UPLOAD_BUFFER_SIZE & UPLOAD_BUFFER_MASK) == 0, "UPLOAD_BUFFER_SIZE must be a power of two");

static const uint32_t TIMESTAMP_QUERY_POOL_SIZE = 256;
static_assert((TIMESTAMP_QUERY_POOL_SIZE & 1) == 0, "TIMESTAMP_QUERY_POOL_SIZE must be an even number");

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT, uint64_t, size_t, int32_t code, const char*, const char* message, void*)
{
    if ((flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) != 0)
    {
        VkError(message);
    }
    else if ((flags & (VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)) != 0)
    {
        printf("Warning: %s\n", message);
    }
    else
    {
        printf("Information: %s\n", message);
    }
    return VK_FALSE;
}

static void CreateSwapchain(uint32_t width, uint32_t height, uint32_t image_count, VkDisplayMode display_mode)
{
    VkSurfaceCapabilitiesKHR surface_capabilities = {};
    VK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Vk.PhysicalDevice, Vk.Surface, &surface_capabilities));
    Vk.SwapchainImageExtent.width = VkMin(VkMax(width, surface_capabilities.minImageExtent.width), surface_capabilities.maxImageExtent.width);
    Vk.SwapchainImageExtent.height = VkMin(VkMax(height, surface_capabilities.minImageExtent.height), surface_capabilities.maxImageExtent.height);
    Vk.SwapchainImageCount = surface_capabilities.maxImageCount == 0 ? VkMax(image_count, surface_capabilities.minImageCount) : VkMin(VkMax(image_count, surface_capabilities.minImageCount), surface_capabilities.maxImageCount);
	
	Vk.DisplayMode = display_mode;

    uint32_t surface_format_count = 0;
    VK(vkGetPhysicalDeviceSurfaceFormatsKHR(Vk.PhysicalDevice, Vk.Surface, &surface_format_count, NULL));
    std::vector<VkSurfaceFormatKHR> surface_formats(surface_format_count);
    VK(vkGetPhysicalDeviceSurfaceFormatsKHR(Vk.PhysicalDevice, Vk.Surface, &surface_format_count, surface_formats.data()));

	bool surface_format_supported = false;
	switch (Vk.DisplayMode)
	{
	case VK_DISPLAY_MODE_SDR:
		for (uint32_t i = 0; i < surface_format_count; ++i)
		{
			if ((surface_formats[i].format == VK_FORMAT_R8G8B8A8_UNORM || surface_formats[i].format == VK_FORMAT_B8G8R8A8_UNORM) && surface_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				Vk.SwapchainSurfaceFormat = surface_formats[i];
				surface_format_supported = true;
				break;
			}
		}
		break;
	case VK_DISPLAY_MODE_HDR10:
		for (uint32_t i = 0; i < surface_format_count; ++i)
		{
			if ((surface_formats[i].format == VK_FORMAT_A2R10G10B10_UNORM_PACK32 || surface_formats[i].format == VK_FORMAT_A2B10G10R10_UNORM_PACK32) && surface_formats[i].colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT)
			{
				Vk.SwapchainSurfaceFormat = surface_formats[i];
				surface_format_supported = true;
				break;
			}
		}
		break;
	case VK_DISPLAY_MODE_SCRGB:
		for (uint32_t i = 0; i < surface_format_count; ++i)
		{
			if ((surface_formats[i].format == VK_FORMAT_R16G16B16A16_SFLOAT) && surface_formats[i].colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT)
			{
				Vk.SwapchainSurfaceFormat = surface_formats[i];
				surface_format_supported = true;
				break;
			}
		}
		break;
	}
	if (!surface_format_supported)
	{
		VkError("Surface format is not supported");
	}

    uint32_t present_mode_count = 0;
    VK(vkGetPhysicalDeviceSurfacePresentModesKHR(Vk.PhysicalDevice, Vk.Surface, &present_mode_count, NULL));
    std::vector<VkPresentModeKHR> present_modes(present_mode_count);
    VK(vkGetPhysicalDeviceSurfacePresentModesKHR(Vk.PhysicalDevice, Vk.Surface, &present_mode_count, present_modes.data()));

    VkSwapchainCreateInfoKHR swapchain_info = {};
    swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_info.surface = Vk.Surface;
    swapchain_info.minImageCount = Vk.SwapchainImageCount;
    swapchain_info.imageFormat = Vk.SwapchainSurfaceFormat.format;
    swapchain_info.imageColorSpace = Vk.SwapchainSurfaceFormat.colorSpace;
    swapchain_info.imageExtent = Vk.SwapchainImageExtent;
    swapchain_info.imageArrayLayers = 1;
    swapchain_info.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_info.preTransform = surface_capabilities.currentTransform;
    swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchain_info.clipped = VK_TRUE;
	swapchain_info.oldSwapchain = Vk.Swapchain;
    VK(vkCreateSwapchainKHR(Vk.Device, &swapchain_info, NULL, &Vk.Swapchain));

	if (swapchain_info.oldSwapchain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(Vk.Device, swapchain_info.oldSwapchain, NULL);
	}

	if (Vk.DisplayMode == VK_DISPLAY_MODE_HDR10 || Vk.DisplayMode == VK_DISPLAY_MODE_SCRGB)
	{
		VkHdrMetadataEXT hdr_metadata = {};
		hdr_metadata.sType = VK_STRUCTURE_TYPE_HDR_METADATA_EXT;
		hdr_metadata.displayPrimaryRed = VkXYColorEXT{ 0.708f, 0.292f };
		hdr_metadata.displayPrimaryGreen = VkXYColorEXT{ 0.170f, 0.797f };
		hdr_metadata.displayPrimaryBlue = VkXYColorEXT{ 0.131f, 0.046f };
		hdr_metadata.whitePoint = VkXYColorEXT{ 0.3127f, 0.3290f };
		hdr_metadata.maxLuminance = 1000.0f;
		hdr_metadata.minLuminance = 0.0f;
		hdr_metadata.maxContentLightLevel = 1000.0f;
		hdr_metadata.maxFrameAverageLightLevel = 400.0f;
		vkSetHdrMetadataEXT(Vk.Device, 1, &Vk.Swapchain, &hdr_metadata);
	}

    VK(vkGetSwapchainImagesKHR(Vk.Device, Vk.Swapchain, &Vk.SwapchainImageCount, NULL));
    std::vector<VkImage> swapchain_images(Vk.SwapchainImageCount);
    VK(vkGetSwapchainImagesKHR(Vk.Device, Vk.Swapchain, &Vk.SwapchainImageCount, swapchain_images.data()));

    Vk.SwapchainImageIndex = 0;
    Vk.SwapchainImages.resize(Vk.SwapchainImageCount);
    Vk.SwapchainImageViews.resize(Vk.SwapchainImageCount);
    for (uint32_t i = 0; i < Vk.SwapchainImageCount; ++i)
    {
        Vk.SwapchainImages[i] = swapchain_images[i];

        VkImageViewCreateInfo image_view_info = {};
        image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_info.image = Vk.SwapchainImages[i];
        image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_info.format = Vk.SwapchainSurfaceFormat.format;
        image_view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_info.subresourceRange.baseMipLevel = 0;
        image_view_info.subresourceRange.levelCount = 1;
        image_view_info.subresourceRange.baseArrayLayer = 0;
        image_view_info.subresourceRange.layerCount = 1;
        VK(vkCreateImageView(Vk.Device, &image_view_info, NULL, &Vk.SwapchainImageViews[i]));

        VkRecordCommands(
            [=](VkCommandBuffer cmd)
            {
                VkImageMemoryBarrier barrier;
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.pNext = NULL;
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
                barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = Vk.SwapchainImages[i];
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
                vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
            });
    }

    Vk.CommandBuffers.resize(Vk.SwapchainImageCount);
    Vk.CommandBufferFences.resize(Vk.SwapchainImageCount);
    Vk.CommandBufferSemaphores.resize(Vk.SwapchainImageCount);
    Vk.PresentSemaphores.resize(Vk.SwapchainImageCount);
    Vk.DescriptorPools.resize(Vk.SwapchainImageCount);
    Vk.UploadBufferTails.resize(Vk.SwapchainImageCount);

    for (uint32_t i = 0; i < Vk.SwapchainImageCount; ++i)
    {
        VkCommandBufferAllocateInfo command_buffer_info = {};
        command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        command_buffer_info.commandPool = Vk.CommandPool;
        command_buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        command_buffer_info.commandBufferCount = 1;
        VK(vkAllocateCommandBuffers(Vk.Device, &command_buffer_info, &Vk.CommandBuffers[i]));

        VkFenceCreateInfo fence_info = {};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VK(vkCreateFence(Vk.Device, &fence_info, NULL, &Vk.CommandBufferFences[i]));

        VkSemaphoreCreateInfo semaphore_info = {};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VK(vkCreateSemaphore(Vk.Device, &semaphore_info, NULL, &Vk.CommandBufferSemaphores[i]));
        VK(vkCreateSemaphore(Vk.Device, &semaphore_info, NULL, &Vk.PresentSemaphores[i]));

        std::vector<VkDescriptorPoolSize> descriptor_pool_sizes =
        {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 65535 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 65535 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 65535 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 65535 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 65535 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 65535 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 65535 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 65535 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 65535 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 65535 },
        };
        if (Vk.IsRayTracingSupported)
        {
            descriptor_pool_sizes.push_back({ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 65535 });
        }
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.poolSizeCount = static_cast<uint32_t>(descriptor_pool_sizes.size());
        pool_info.pPoolSizes = descriptor_pool_sizes.data();
        pool_info.maxSets = 65535;
        VK(vkCreateDescriptorPool(Vk.Device, &pool_info, NULL, &Vk.DescriptorPools[i]));

        Vk.UploadBufferTails[i] = 0;
    }

	Vk.UploadBufferHead = 0;

    Vk.FrameIndexCurr = 0;
    Vk.FrameIndexNext = (Vk.FrameIndexCurr + 1) % Vk.SwapchainImageCount;
}
static void DestroySwapchain()
{
    for (uint32_t i = 0; i < Vk.SwapchainImageCount; ++i)
    {
        vkDestroyDescriptorPool(Vk.Device, Vk.DescriptorPools[i], NULL);

        vkDestroySemaphore(Vk.Device, Vk.PresentSemaphores[i], NULL);
        vkDestroySemaphore(Vk.Device, Vk.CommandBufferSemaphores[i], NULL);
        vkDestroyFence(Vk.Device, Vk.CommandBufferFences[i], NULL);
        vkFreeCommandBuffers(Vk.Device, Vk.CommandPool, 1, &Vk.CommandBuffers[i]);

        vkDestroyImageView(Vk.Device, Vk.SwapchainImageViews[i], NULL);
    }
}

void VkInitialize(const VkInitializeParams& params)
{
    VK(volkInitialize());

	std::vector<const char*> instance_extensions =
	{
		VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(VK_USE_PLATFORM_WIN32_KHR)
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
		VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#endif
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
	};
    std::vector<const char*> device_extensions =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};
	const char* validation_layer = "VK_LAYER_KHRONOS_validation";

	uint32_t instance_extension_properties_count = 0;
	VK(vkEnumerateInstanceExtensionProperties(NULL, &instance_extension_properties_count, NULL));
	std::vector<VkExtensionProperties> instance_extension_properties(instance_extension_properties_count);
    VK(vkEnumerateInstanceExtensionProperties(NULL, &instance_extension_properties_count, instance_extension_properties.data()));

	for (uint32_t i = 0; i < static_cast<uint32_t>(instance_extensions.size()); ++i)
	{
		bool extension_supported = false;
		for (uint32_t j = 0; j < instance_extension_properties_count; ++j)
		{
			if (strcmp(instance_extensions[i], instance_extension_properties[j].extensionName) == 0)
			{
				extension_supported = true;
				break;
			}
		}
        if (!extension_supported)
        {
            VkError("All instance extensions are not supported");
        }
	}

    if (params.EnableValidationLayer)
    {
        uint32_t instance_layer_properties_count = 0;
        VK(vkEnumerateInstanceLayerProperties(&instance_layer_properties_count, NULL));
        std::vector<VkLayerProperties> instance_layer_properties(instance_layer_properties_count);
        VK(vkEnumerateInstanceLayerProperties(&instance_layer_properties_count, instance_layer_properties.data()));

        bool validation_layer_supported = false;
        for (uint32_t i = 0; i < instance_layer_properties_count; ++i)
        {
            if (strcmp(validation_layer, instance_layer_properties[i].layerName) == 0)
            {
                validation_layer_supported = true;
                break;
            }
        }
        if (!validation_layer_supported)
        {
            VkError("Validation layer is not supported");
        }
    }

	VkApplicationInfo app_info = {};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.apiVersion = VK_API_VERSION_1_2;

	VkInstanceCreateInfo instance_info = {};
	instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instance_info.pApplicationInfo = &app_info;
	instance_info.enabledExtensionCount = static_cast<uint32_t>(instance_extensions.size());
	instance_info.ppEnabledExtensionNames = instance_extensions.data();
    if (params.EnableValidationLayer)
    {
        instance_info.enabledLayerCount = 1;
        instance_info.ppEnabledLayerNames = &validation_layer;
    }
	VK(vkCreateInstance(&instance_info, NULL, &Vk.Instance));

    volkLoadInstance(Vk.Instance);

    Vk.DebugCallback = VK_NULL_HANDLE;
    if (params.EnableValidationLayer)
    {
        VkDebugReportCallbackCreateInfoEXT callback_info = {};
        callback_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        callback_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
        callback_info.pfnCallback = DebugCallback;
        VK(vkCreateDebugReportCallbackEXT(Vk.Instance, &callback_info, NULL, &Vk.DebugCallback));
    }

#if defined(VK_USE_PLATFORM_WIN32_KHR)
	VkWin32SurfaceCreateInfoKHR surface_info = {};
	surface_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surface_info.hwnd = static_cast<HWND>(params.WindowHandle);
	surface_info.hinstance = static_cast<HINSTANCE>(params.DisplayHandle);
	VK(vkCreateWin32SurfaceKHR(Vk.Instance, &surface_info, NULL, &Vk.Surface));
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
	VkXlibSurfaceCreateInfoKHR surface_info = {};
	surface_info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
	surface_info.window = reinterpret_cast<Window>(params.WindowHandle);
	surface_info.dpy = static_cast<Display*>(params.DisplayHandle);
	VK(vkCreateXlibSurfaceKHR(Vk.Instance, &surface_info, NULL, &Vk.Surface));
#endif

	uint32_t physical_device_count = 0;
	VK(vkEnumeratePhysicalDevices(Vk.Instance, &physical_device_count, NULL));
	std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    VK(vkEnumeratePhysicalDevices(Vk.Instance, &physical_device_count, physical_devices.data()));

	Vk.PhysicalDevice = VK_NULL_HANDLE;
	Vk.GraphicsQueueIndex = ~0U;
	for (size_t i = 0; i < physical_devices.size(); ++i)
	{
		uint32_t device_extension_properties_count = 0;
		VK(vkEnumerateDeviceExtensionProperties(physical_devices[i], NULL, &device_extension_properties_count, NULL));
		std::vector<VkExtensionProperties> device_extension_properties(device_extension_properties_count);
		VK(vkEnumerateDeviceExtensionProperties(physical_devices[i], NULL, &device_extension_properties_count, device_extension_properties.data()));

		bool all_extensions_supported = true;
		for (uint32_t j = 0; j < static_cast<uint32_t>(device_extensions.size()); ++j)
		{
			bool extension_supported = false;
			for (uint32_t k = 0; k < device_extension_properties_count; ++k)
			{
				if (strcmp(device_extensions[j], device_extension_properties[k].extensionName) == 0)
				{
					extension_supported = true;
					break;
				}
			}
			if (!extension_supported)
			{
				all_extensions_supported = false;
				break;
			}
		}
		if (!all_extensions_supported)
			continue;

        if (params.EnableValidationLayer)
        {
            uint32_t device_layer_properties_count = 0;
            VK(vkEnumerateDeviceLayerProperties(physical_devices[i], &device_layer_properties_count, NULL));
            std::vector<VkLayerProperties> device_layer_properties(device_layer_properties_count);
            VK(vkEnumerateDeviceLayerProperties(physical_devices[i], &device_layer_properties_count, device_layer_properties.data()));

            bool validation_layer_supported = false;
            for (uint32_t j = 0; j < device_layer_properties_count; ++j)
            {
                if (strcmp(validation_layer, device_layer_properties[j].layerName) == 0)
                {
                    validation_layer_supported = true;
                    break;
                }
            }
            if (!validation_layer_supported)
                continue;
        }

		uint32_t queue_family_properties_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physical_devices[i], &queue_family_properties_count, NULL);
		std::vector<VkQueueFamilyProperties> queue_family_properties(queue_family_properties_count);
		vkGetPhysicalDeviceQueueFamilyProperties(physical_devices[i], &queue_family_properties_count, queue_family_properties.data());

		for (uint32_t j = 0; j < queue_family_properties_count; ++j)
		{
			VkBool32 queue_flags_supported = (queue_family_properties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;

			VkBool32 surface_supported = VK_FALSE;
			VK(vkGetPhysicalDeviceSurfaceSupportKHR(physical_devices[i], j, Vk.Surface, &surface_supported));

			if (queue_flags_supported && surface_supported)
			{
                Vk.GraphicsQueueIndex = j;
				break;
			}
		}
		if (Vk.GraphicsQueueIndex == ~0U)
			continue;

		VkPhysicalDeviceProperties device_properties;
		vkGetPhysicalDeviceProperties(physical_devices[i], &device_properties);
		if (VK_VERSION_MAJOR(device_properties.apiVersion) < 1 || (VK_VERSION_MAJOR(device_properties.apiVersion) == 1 && VK_VERSION_MINOR(device_properties.apiVersion) < 2))
			continue;

        Vk.PhysicalDevice = physical_devices[i];
		break;
	}
    if (Vk.PhysicalDevice == VK_NULL_HANDLE)
    {
        VkError("Error: No compatible physical device was found");
    }

    vkGetPhysicalDeviceProperties(Vk.PhysicalDevice, &Vk.PhysicalDeviceProperties);

    uint32_t device_extension_properties_count = 0;
    VK(vkEnumerateDeviceExtensionProperties(Vk.PhysicalDevice, NULL, &device_extension_properties_count, NULL));
    std::vector<VkExtensionProperties> device_extension_properties(device_extension_properties_count);
    VK(vkEnumerateDeviceExtensionProperties(Vk.PhysicalDevice, NULL, &device_extension_properties_count, device_extension_properties.data()));

	// Check if HDR display modes are supported
	{
		std::vector<const char*> hdr_display_extensions =
		{
			 VK_EXT_HDR_METADATA_EXTENSION_NAME,
		};

		bool is_hdr_display_supported = true;
		for (size_t i = 0; i < hdr_display_extensions.size(); ++i)
		{
			bool is_extension_supported = false;
			for (uint32_t j = 0; j < device_extension_properties_count; ++j)
			{
				if (strcmp(device_extension_properties[j].extensionName, hdr_display_extensions[i]) == 0)
				{
					is_extension_supported = true;
					break;
				}
			}
			if (!is_extension_supported)
			{
				is_hdr_display_supported = false;
				break;
			}
		}
		if (is_hdr_display_supported)
		{
			for (size_t i = 0; i < hdr_display_extensions.size(); ++i)
			{
				device_extensions.push_back(hdr_display_extensions[i]);
			}
		}

		uint32_t surface_format_count = 0;
		VK(vkGetPhysicalDeviceSurfaceFormatsKHR(Vk.PhysicalDevice, Vk.Surface, &surface_format_count, NULL));
		std::vector<VkSurfaceFormatKHR> surface_formats(surface_format_count);
		VK(vkGetPhysicalDeviceSurfaceFormatsKHR(Vk.PhysicalDevice, Vk.Surface, &surface_format_count, surface_formats.data()));

		memset(Vk.IsDisplayModeSupported, 0, sizeof(Vk.IsDisplayModeSupported));
		for (uint32_t i = 0; i < surface_format_count; ++i)
		{
			Vk.IsDisplayModeSupported[VK_DISPLAY_MODE_SDR] |= ((surface_formats[i].format == VK_FORMAT_R8G8B8A8_UNORM || surface_formats[i].format == VK_FORMAT_B8G8R8A8_UNORM) && surface_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
			if (is_hdr_display_supported)
			{
				Vk.IsDisplayModeSupported[VK_DISPLAY_MODE_HDR10] |= ((surface_formats[i].format == VK_FORMAT_A2R10G10B10_UNORM_PACK32 || surface_formats[i].format == VK_FORMAT_A2B10G10R10_UNORM_PACK32) && surface_formats[i].colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT);
				Vk.IsDisplayModeSupported[VK_DISPLAY_MODE_SCRGB] |= ((surface_formats[i].format == VK_FORMAT_R16G16B16A16_SFLOAT) && surface_formats[i].colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT);
			}
		}
	}

	// Check if ray tracing is supported
	{
		Vk.IsRayTracingSupported = true;

		std::vector<const char*> ray_tracing_extensions =
		{
			VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
			VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
			VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
			VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
			VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
			VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
		};
		for (size_t i = 0; i < ray_tracing_extensions.size(); ++i)
		{
			bool is_extension_supported = false;
			for (uint32_t j = 0; j < device_extension_properties_count; ++j)
			{
				if (strcmp(device_extension_properties[j].extensionName, ray_tracing_extensions[i]) == 0)
				{
					is_extension_supported = true;
					break;
				}
			}
			if (!is_extension_supported)
			{
				Vk.IsRayTracingSupported = false;
				break;
			}
		}
		if (Vk.IsRayTracingSupported)
		{
			for (size_t i = 0; i < ray_tracing_extensions.size(); ++i)
			{
				device_extensions.push_back(ray_tracing_extensions[i]);
			}
		}
	}

	const float queue_priority = 1.0f;
	VkDeviceQueueCreateInfo queue_info = {};
	queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_info.queueFamilyIndex = Vk.GraphicsQueueIndex;
	queue_info.queueCount = 1;
	queue_info.pQueuePriorities = &queue_priority;

    VkPhysicalDeviceFeatures device_features = {};
	device_features.samplerAnisotropy = VK_TRUE;
    device_features.shaderStorageImageExtendedFormats = VK_TRUE;

	VkPhysicalDeviceVulkan12Features device_vulkan_1_2_features = {};
	device_vulkan_1_2_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	device_vulkan_1_2_features.bufferDeviceAddress = VK_TRUE;
	device_vulkan_1_2_features.runtimeDescriptorArray = VK_TRUE;
	device_vulkan_1_2_features.descriptorIndexing = VK_TRUE;
	device_vulkan_1_2_features.descriptorBindingPartiallyBound = VK_TRUE;

	VkPhysicalDeviceAccelerationStructureFeaturesKHR device_acceleration_structure_features = {};
	device_acceleration_structure_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	device_acceleration_structure_features.pNext = &device_vulkan_1_2_features;
	device_acceleration_structure_features.accelerationStructure = VK_TRUE;

	VkPhysicalDeviceRayTracingPipelineFeaturesKHR device_ray_tracing_pipeline_features = {};
	device_ray_tracing_pipeline_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
	device_ray_tracing_pipeline_features.pNext = &device_acceleration_structure_features;
	device_ray_tracing_pipeline_features.rayTracingPipeline = VK_TRUE;

	VkDeviceCreateInfo device_info = {};
	device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pNext = Vk.IsRayTracingSupported ? &device_ray_tracing_pipeline_features : NULL;
	device_info.queueCreateInfoCount = 1;
	device_info.pQueueCreateInfos = &queue_info;
    device_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
	device_info.ppEnabledExtensionNames = device_extensions.data();
    device_info.pEnabledFeatures = &device_features;
    if (params.EnableValidationLayer)
    {
        device_info.enabledLayerCount = 1;
        device_info.ppEnabledLayerNames = &validation_layer;
    }
	VK(vkCreateDevice(Vk.PhysicalDevice, &device_info, NULL, &Vk.Device));

    volkLoadDevice(Vk.Device);

	vkGetDeviceQueue(Vk.Device, Vk.GraphicsQueueIndex, 0, &Vk.GraphicsQueue);

    VkCommandPoolCreateInfo command_pool_info = {};
    command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    command_pool_info.queueFamilyIndex = Vk.GraphicsQueueIndex;
    VK(vkCreateCommandPool(Vk.Device, &command_pool_info, NULL, &Vk.CommandPool));

	VmaAllocatorCreateInfo allocator_info = {};
	allocator_info.flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT | (Vk.IsRayTracingSupported ? VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT : 0);
	allocator_info.physicalDevice = Vk.PhysicalDevice;
	allocator_info.device = Vk.Device;
	allocator_info.instance = Vk.Instance;
	allocator_info.pAllocationCallbacks = NULL;
	VK(vmaCreateAllocator(&allocator_info, &Vk.Allocator));

	VkBufferCreateInfo upload_buffer_info = {};
	upload_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	upload_buffer_info.size = UPLOAD_BUFFER_SIZE;
	upload_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	upload_buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo upload_buffer_allocation_info = {};
    upload_buffer_allocation_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    upload_buffer_allocation_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

	VmaAllocationInfo allocation_info = {};
	VK(vmaCreateBuffer(Vk.Allocator, &upload_buffer_info, &upload_buffer_allocation_info, &Vk.UploadBuffer, &Vk.UploadBufferAllocation, &allocation_info));
	Vk.UploadBufferMemory = allocation_info.deviceMemory;
	Vk.UploadBufferMemoryOffset = allocation_info.offset;
	Vk.UploadBufferMappedData = (uint8_t*)allocation_info.pMappedData;

	Vk.Swapchain = VK_NULL_HANDLE;
    CreateSwapchain(params.BackBufferWidth, params.BackBufferHeight, params.DesiredBackBufferCount, params.DisplayMode);

	VkQueryPoolCreateInfo timestamp_query_pool_info = {};
	timestamp_query_pool_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	timestamp_query_pool_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
	timestamp_query_pool_info.queryCount = TIMESTAMP_QUERY_POOL_SIZE;
	VK(vkCreateQueryPool(Vk.Device, &timestamp_query_pool_info, NULL, &Vk.TimestampQueryPool));
	Vk.TimestampQueryPoolOffset = 0;
}
void VkTerminate()
{
	DestroySwapchain();
	vkDestroySwapchainKHR(Vk.Device, Vk.Swapchain, NULL);
    
	vkDestroyQueryPool(Vk.Device, Vk.TimestampQueryPool, NULL);
	vmaDestroyBuffer(Vk.Allocator, Vk.UploadBuffer, Vk.UploadBufferAllocation);
	vmaDestroyAllocator(Vk.Allocator);
    vkDestroyCommandPool(Vk.Device, Vk.CommandPool, NULL);
	vkDestroyDevice(Vk.Device, NULL);
	vkDestroySurfaceKHR(Vk.Instance, Vk.Surface, NULL);
    if (Vk.DebugCallback != VK_NULL_HANDLE)
    {
        vkDestroyDebugReportCallbackEXT(Vk.Instance, Vk.DebugCallback, NULL);
    }
	vkDestroyInstance(Vk.Instance, NULL);
}

void VkResize(uint32_t width, uint32_t height, VkDisplayMode display_mode)
{
	DestroySwapchain();
    CreateSwapchain(width, height, Vk.SwapchainImageCount, display_mode);
}

VkAllocation VkAllocateUploadBuffer(VkDeviceSize size, VkDeviceSize alignment)
{
    VkDeviceSize head = Vk.UploadBufferHead;
    VkDeviceSize tail = Vk.UploadBufferTails[Vk.FrameIndexNext];

    VkDeviceSize offset = (head + alignment - 1) & ~(alignment - 1);
    if (offset + size > UPLOAD_BUFFER_SIZE)
    {
        offset = 0;
    }

    if (offset + size > UPLOAD_BUFFER_SIZE || ((head - tail) & UPLOAD_BUFFER_MASK) > ((offset + size - tail) & UPLOAD_BUFFER_MASK))
    {
        VkError("Upload buffer is out of memory");
    }

    Vk.UploadBufferHead = offset + size;

    VkAllocation allocation;
    allocation.Buffer = Vk.UploadBuffer;
    allocation.Offset = offset;
    allocation.Data = Vk.UploadBufferMappedData + offset;
    return allocation;
}

void VkRecordCommands(const std::function<void(VkCommandBuffer)>& commands)
{
    Vk.RecordedCommands.emplace_back(commands);
}

VkCommandBuffer VkBeginFrame()
{
    VK(vkAcquireNextImageKHR(Vk.Device, Vk.Swapchain, UINT64_MAX, Vk.PresentSemaphores[Vk.FrameIndexCurr], VK_NULL_HANDLE, &Vk.SwapchainImageIndex));

    VK(vkWaitForFences(Vk.Device, 1, &Vk.CommandBufferFences[Vk.FrameIndexCurr], VK_TRUE, UINT64_MAX));
    VK(vkResetFences(Vk.Device, 1, &Vk.CommandBufferFences[Vk.FrameIndexCurr]));

    VK(vkResetDescriptorPool(Vk.Device, Vk.DescriptorPools[Vk.FrameIndexCurr], 0));

    VkCommandBuffer cmd = Vk.CommandBuffers[Vk.FrameIndexCurr];

    VkCommandBufferBeginInfo cmd_begin_info = {};
    cmd_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK(vkBeginCommandBuffer(cmd, &cmd_begin_info));

    for (const std::function<void(VkCommandBuffer)>& commands : Vk.RecordedCommands)
    {
        commands(cmd);
    }
    Vk.RecordedCommands.clear();

	while (!Vk.TimestampLabelsInFlight.empty())
	{
		const std::pair<std::string, uint32_t>& label = Vk.TimestampLabelsInFlight.front();

		uint64_t timestamps[2];
		VkResult result = vkGetQueryPoolResults(Vk.Device, Vk.TimestampQueryPool, label.second, 2, sizeof(uint64_t) * 2, timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
		assert(result == VK_SUCCESS || result == VK_NOT_READY);
		if (result == VK_NOT_READY)
		{
			break;
		}

		const double timestamp_period = static_cast<double>(Vk.PhysicalDeviceProperties.limits.timestampPeriod) * 1e-6;
		float duration = static_cast<float>(static_cast<double>(timestamps[1] - timestamps[0]) * timestamp_period);
		if (Vk.TimestampLabelsResult.find(label.first) == Vk.TimestampLabelsResult.end())
		{
			Vk.TimestampLabelsResult[label.first] = 0.0f;
		}
		Vk.TimestampLabelsResult[label.first] += (duration - Vk.TimestampLabelsResult[label.first]) * 0.25f;

		Vk.TimestampLabelsInFlight.erase(Vk.TimestampLabelsInFlight.begin());
	}

    VkImageMemoryBarrier barrier;
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.pNext = NULL;
    barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = Vk.SwapchainImages[Vk.SwapchainImageIndex];
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

    return cmd;
}
void VkEndFrame()
{
    VkCommandBuffer cmd = Vk.CommandBuffers[Vk.FrameIndexCurr];

    VkImageMemoryBarrier barrier;
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.pNext = NULL;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = Vk.SwapchainImages[Vk.SwapchainImageIndex];
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

    VK(vkEndCommandBuffer(cmd));

    Vk.UploadBufferTails[Vk.FrameIndexCurr] = Vk.UploadBufferHead;

	assert(Vk.TimestampLabelsPushed.empty());

    VkPipelineStageFlags wait_stage_flags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &Vk.PresentSemaphores[Vk.FrameIndexCurr];
    submit_info.pWaitDstStageMask = &wait_stage_flags;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &Vk.CommandBufferSemaphores[Vk.FrameIndexCurr];
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &Vk.CommandBuffers[Vk.FrameIndexCurr];
    VK(vkQueueSubmit(Vk.GraphicsQueue, 1, &submit_info, Vk.CommandBufferFences[Vk.FrameIndexCurr]));

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &Vk.CommandBufferSemaphores[Vk.FrameIndexCurr];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &Vk.Swapchain;
    present_info.pImageIndices = &Vk.SwapchainImageIndex;
    VK(vkQueuePresentKHR(Vk.GraphicsQueue, &present_info));

    Vk.FrameIndexCurr = Vk.FrameIndexNext;
    Vk.FrameIndexNext = (Vk.FrameIndexCurr + 1) % Vk.SwapchainImageCount;
}

VkDescriptorSet VkCreateDescriptorSetForCurrentFrame(VkDescriptorSetLayout layout, std::initializer_list<VkDescriptorSetEntry> entries)
{
    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = Vk.DescriptorPools[Vk.FrameIndexCurr];
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &layout;

    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    VK(vkAllocateDescriptorSets(Vk.Device, &alloc_info, &descriptor_set));

    std::vector<VkWriteDescriptorSet> write_info(entries.size());
    for (size_t i = 0; i < entries.size(); ++i)
    {
        const VkDescriptorSetEntry& entry = *(entries.begin() + i);

        write_info[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write_info[i].dstSet = descriptor_set;
        write_info[i].dstBinding = entry.Binding;
        write_info[i].dstArrayElement = entry.ArrayIndex;
        write_info[i].descriptorCount = entry.ArrayCount;
        write_info[i].descriptorType = entry.Type;
        switch (entry.Type)
        {
            case VK_DESCRIPTOR_TYPE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                write_info[i].pImageInfo = entry.ArrayCount > 1 ? entry.ImageInfos : &entry.ImageInfo;
                break;
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                write_info[i].pBufferInfo = entry.ArrayCount > 1 ? entry.BufferInfos : &entry.BufferInfo;
                break;
            case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                write_info[i].pTexelBufferView = entry.ArrayCount > 1 ? entry.TexelBufferInfos : &entry.TexelBufferInfo;
                break;
            case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
                write_info[i].pNext = entry.ArrayCount > 1 ? entry.AccelerationStructureInfos : &entry.AccelerationStructureInfo;
                break;
        }
    }
    vkUpdateDescriptorSets(Vk.Device, static_cast<uint32_t>(write_info.size()), write_info.data(), 0, nullptr);

    return descriptor_set;
}

VkDescriptorSetEntry::VkDescriptorSetEntry(uint32_t binding, VkDescriptorType type, uint32_t array_index, VkImageView image_view, VkImageLayout image_layout, VkSampler sampler)
{
    Binding = binding;
    Type = type;
	ArrayIndex = array_index;
	ArrayCount = 1;
    ImageInfo.sampler = sampler;
    ImageInfo.imageView = image_view;
    ImageInfo.imageLayout = image_layout;
}
VkDescriptorSetEntry::VkDescriptorSetEntry(uint32_t binding, VkDescriptorType type, uint32_t array_index, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size)
{
    Binding = binding;
    Type = type;
	ArrayIndex = array_index;
	ArrayCount = 1;
    BufferInfo.buffer = buffer;
    BufferInfo.offset = offset;
    BufferInfo.range = size;
}
VkDescriptorSetEntry::VkDescriptorSetEntry(uint32_t binding, VkDescriptorType type, uint32_t array_index, VkAccelerationStructureKHR acceleration_structure)
{
    Binding = binding;
    Type = type;
	ArrayIndex = array_index;
	ArrayCount = 1;
    AccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    AccelerationStructureInfo.pNext = NULL;
    AccelerationStructureInfo.accelerationStructureCount = 1;
    AccelerationStructureInfo.pAccelerationStructures = &acceleration_structure;
}
VkDescriptorSetEntry::VkDescriptorSetEntry(uint32_t binding, VkDescriptorType type, uint32_t array_index, uint32_t array_count, const VkDescriptorImageInfo* infos)
{
	assert(array_count > 0);
	Binding = binding;
	Type = type;
	ArrayIndex = array_index;
	ArrayCount = array_count;
	if (array_count == 1)
	{
		ImageInfo = infos[0];
	}
	else
	{
		ImageInfos = infos;
	}
}
VkDescriptorSetEntry::VkDescriptorSetEntry(uint32_t binding, VkDescriptorType type, uint32_t array_index, uint32_t array_count, const VkDescriptorBufferInfo* infos)
{
	assert(array_count > 0);
	Binding = binding;
	Type = type;
	ArrayIndex = array_index;
	ArrayCount = array_count;
	if (array_count == 1)
	{
		BufferInfo = infos[0];
	}
	else
	{
		BufferInfos = infos;
	}
}

void VkPushLabel(VkCommandBuffer cmd, const std::string& label)
{
	const uint32_t query = Vk.TimestampQueryPoolOffset;
	Vk.TimestampQueryPoolOffset += 2;
	if (Vk.TimestampQueryPoolOffset >= TIMESTAMP_QUERY_POOL_SIZE)
		Vk.TimestampQueryPoolOffset = 0;
	Vk.TimestampLabelsPushed.push_back({ label, query });
    vkCmdResetQueryPool(cmd, Vk.TimestampQueryPool, query, 2);
	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, Vk.TimestampQueryPool, query);
}
void VkPopLabel(VkCommandBuffer cmd)
{
	assert(!Vk.TimestampLabelsPushed.empty());
	const std::pair<std::string, uint32_t>& label = Vk.TimestampLabelsPushed.back();
	Vk.TimestampLabelsInFlight.emplace_back(label);
	Vk.TimestampLabelsPushed.pop_back();
	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, Vk.TimestampQueryPool, label.second + 1);
}
float VkGetLabel(const std::string& label)
{
	auto label_itr = Vk.TimestampLabelsResult.find(label);
	if (label_itr != Vk.TimestampLabelsResult.end())
		return label_itr->second;
	return 0.0f;
}