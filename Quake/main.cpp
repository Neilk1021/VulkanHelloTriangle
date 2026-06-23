#include <algorithm>
#include <assert.h>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "vertex.hpp"
#include "uniformBuffer.hpp"
#include <glm/gtc/matrix_transform.hpp>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#define GLFW_INCLUDE_VULKAN        // REQUIRED only for GLFW CreateWindowSurface.
#include <GLFW/glfw3.h>

constexpr uint32_t WIDTH  = 800;
constexpr uint32_t HEIGHT = 600;

const std::vector<char const *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

constexpr int MAX_FRAMES_IN_FLIGHT = 2;


class HelloTriangleApplication {
public:
	void run()
	{
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

private:
	GLFWwindow                      *window = nullptr;
	vk::raii::Context                context;
	vk::raii::Instance               instance       = nullptr;
	vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
	vk::raii::SurfaceKHR             surface        = nullptr;
	vk::raii::PhysicalDevice         physicalDevice = nullptr;
	vk::raii::Device                 device         = nullptr;
	vk::raii::Queue                  queue          = nullptr;
	vk::raii::SwapchainKHR           swapChain      = nullptr;
	std::vector<vk::Image>           swapChainImages;
	vk::SurfaceFormatKHR             swapChainSurfaceFormat;
	vk::Extent2D                     swapChainExtent;
	std::vector<vk::raii::ImageView> swapChainImageViews;

	uint32_t graphicsQueueFamilyIndex = ~0;

	vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
	vk::raii::DescriptorPool descriptorPool = nullptr;
	std::vector<vk::raii::DescriptorSet> descriptorSets;

	vk::raii::PipelineLayout pipelineLayout = nullptr;
	vk::raii::Pipeline graphicsPipeline = nullptr;
	vk::raii::CommandPool commandPool = nullptr;

	vk::raii::Buffer vertexBuffer = nullptr;
	vk::raii::DeviceMemory vertexBufferMemory = nullptr;
	vk::raii::Buffer       indexBuffer        = nullptr;
	vk::raii::DeviceMemory indexBufferMemory  = nullptr;

	std::vector<vk::raii::Buffer>       uniformBuffers;
	std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
	std::vector<void *>                 uniformBuffersMapped;

	std::vector<vk::raii::CommandBuffer> commandBuffers;

	std::vector<vk::raii::Semaphore> presentCompleteSemaphores;
	std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
	std::vector<vk::raii::Fence> inFlightFences;

	vk::raii::Image        textureImage       = nullptr;
	vk::raii::DeviceMemory textureImageMemory = nullptr;
	vk::raii::ImageView    textureImageView   = nullptr;
	vk::raii::Sampler      textureSampler     = nullptr;

	const std::vector<Vertex> vertices = {
		{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
		{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
		{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
		{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
	};

	const std::vector<uint16_t> indices = {
		0, 1, 2, 2, 3, 0
	};

	uint32_t frameIndex = 0;
	bool framebufferResized = false;

	std::vector<const char *> requiredDeviceExtension = {
		vk::KHRSwapchainExtensionName};

	void initWindow()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
	}

	static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
	{
		auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
		app->framebufferResized = true;
	}


	void initVulkan()
	{
		createInstance();
		setupDebugMessenger();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createImageViews();
		createDescriptorSetLayout();
		createGraphicsPipeline();
		createCommandPool();
		createTextureImage();
		createTextureImageView();
		createTextureSampler();
		createVertexBuffer();
		createIndexBuffer();
		createUniformBuffer();
		createDescriptorPool();
		createDescriptorSets();
		createCommandBuffer();
		createSyncObjects();
	}

	void mainLoop()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
			drawFrame();
		}

		device.waitIdle();
	}

	void cleanup()
	{
		cleanupSwapChain();
		glfwDestroyWindow(window);
		glfwTerminate();
	}

	void createInstance()
	{
		constexpr vk::ApplicationInfo appInfo{.pApplicationName   = "Hello Triangle",
											  .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
											  .pEngineName        = "No Engine",
											  .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
											  .apiVersion         = vk::ApiVersion14};

		// Get the required layers
		std::vector<char const *> requiredLayers;
		if (enableValidationLayers)
		{
			requiredLayers.assign(validationLayers.begin(), validationLayers.end());
		}

		// Check if the required layers are supported by the Vulkan implementation.
		auto layerProperties    = context.enumerateInstanceLayerProperties();
		auto unsupportedLayerIt = std::ranges::find_if(requiredLayers,
													   [&layerProperties](auto const &requiredLayer) {
														   return std::ranges::none_of(layerProperties,
																					   [requiredLayer](auto const &layerProperty) { return strcmp(layerProperty.layerName, requiredLayer) == 0; });
													   });
		if (unsupportedLayerIt != requiredLayers.end())
		{
			throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
		}

		// Get the required extensions.
		auto requiredExtensions = getRequiredInstanceExtensions();

		// Check if the required extensions are supported by the Vulkan implementation.
		auto extensionProperties = context.enumerateInstanceExtensionProperties();
		auto unsupportedPropertyIt =
			std::ranges::find_if(requiredExtensions,
								 [&extensionProperties](auto const &requiredExtension) {
									 return std::ranges::none_of(extensionProperties,
																 [requiredExtension](auto const &extensionProperty) { return strcmp(extensionProperty.extensionName, requiredExtension) == 0; });
								 });
		if (unsupportedPropertyIt != requiredExtensions.end())
		{
			throw std::runtime_error("Required extension not supported: " + std::string(*unsupportedPropertyIt));
		}

		vk::InstanceCreateInfo createInfo{.pApplicationInfo        = &appInfo,
										  .enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size()),
										  .ppEnabledLayerNames     = requiredLayers.data(),
										  .enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size()),
										  .ppEnabledExtensionNames = requiredExtensions.data()};
		instance = vk::raii::Instance(context, createInfo);
	}

	void setupDebugMessenger()
	{
		if (!enableValidationLayers)
			return;

		vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
															vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
		vk::DebugUtilsMessageTypeFlagsEXT     messageTypeFlags(
			vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
		vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{.messageSeverity = severityFlags,
																			  .messageType     = messageTypeFlags,
																			  .pfnUserCallback = &debugCallback};
		debugMessenger = instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
	}

	void createSurface()
	{
		VkSurfaceKHR _surface;
		if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0)
		{
			throw std::runtime_error("failed to create window surface!");
		}
		surface = vk::raii::SurfaceKHR(instance, _surface);
	}

	bool isDeviceSuitable(vk::raii::PhysicalDevice const &physicalDevice)
	{
		// Check if the physicalDevice supports the Vulkan 1.3 API version
		bool supportsVulkan1_3 = physicalDevice.getProperties().apiVersion >= VK_API_VERSION_1_3;

		// Check if any of the queue families support graphics operations
		auto queueFamilies    = physicalDevice.getQueueFamilyProperties();
		bool supportsGraphics = std::ranges::any_of(queueFamilies, [](auto const &qfp) { return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics); });

		// Check if all required physicalDevice extensions are available
		auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
		bool supportsAllRequiredExtensions =
			std::ranges::all_of(requiredDeviceExtension,
								[&availableDeviceExtensions](auto const &requiredDeviceExtension) {
									return std::ranges::any_of(availableDeviceExtensions,
															   [requiredDeviceExtension](auto const &availableDeviceExtension) { return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0; });
								});

		// Check if the physicalDevice supports the required features
		auto features                 = physicalDevice.template getFeatures2<vk::PhysicalDeviceFeatures2,
																			 vk::PhysicalDeviceVulkan11Features,
																			 vk::PhysicalDeviceVulkan13Features,
																			 vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
		bool supportsRequiredFeatures =
			features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy &&
			features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
			features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
			features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

		// Return true if the physicalDevice meets all the criteria
		return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
	}

	void pickPhysicalDevice()
	{
		std::vector<vk::raii::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
		auto const                            devIter         = std::ranges::find_if(physicalDevices, [&](auto const &physicalDevice) { return isDeviceSuitable(physicalDevice); });
		if (devIter == physicalDevices.end())
		{
			throw std::runtime_error("failed to find a suitable GPU!");
		}
		physicalDevice = *devIter;
	}

	void createLogicalDevice()
	{
		std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

		// get the first index into queueFamilyProperties which supports both graphics and present

		graphicsQueueFamilyIndex = ~0;
		for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
		{
			if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
				physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface))
			{
				// found a queue family that supports both graphics and present
				graphicsQueueFamilyIndex = qfpIndex;
				break;
			}
		}
		if (graphicsQueueFamilyIndex == ~0)
		{
			throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
		}

		// query for Vulkan 1.3 features
		vk::StructureChain<vk::PhysicalDeviceFeatures2,
						   vk::PhysicalDeviceVulkan11Features,
						   vk::PhysicalDeviceVulkan13Features,
						   vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
			featureChain = {
			{.features = {.samplerAnisotropy = VK_TRUE}},                                    // vk::PhysicalDeviceFeatures2
			{.shaderDrawParameters = true},        // vk::PhysicalDeviceVulkan11Features
			{
				.synchronization2 = VK_TRUE,
				.dynamicRendering = true,
				},									// vk::PhysicalDeviceVulkan13Features
		 {
			.extendedDynamicState = true}         // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
			};

		// create a Device
		float                     queuePriority = 0.5f;
		vk::DeviceQueueCreateInfo deviceQueueCreateInfo{.queueFamilyIndex = graphicsQueueFamilyIndex, .queueCount = 1, .pQueuePriorities = &queuePriority};
		vk::DeviceCreateInfo      deviceCreateInfo{.pNext                   = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
												   .queueCreateInfoCount    = 1,
												   .pQueueCreateInfos       = &deviceQueueCreateInfo,
												   .enabledExtensionCount   = static_cast<uint32_t>(requiredDeviceExtension.size()),
												   .ppEnabledExtensionNames = requiredDeviceExtension.data()};

		device = vk::raii::Device(physicalDevice, deviceCreateInfo);
		queue  = vk::raii::Queue(device, graphicsQueueFamilyIndex, 0);
	}

	void recreateSwapChain()
	{
		int width = 0, height = 0;
		glfwGetFramebufferSize(window, &width, &height);
		while (width == 0 || height == 0) {
			glfwGetFramebufferSize(window, &width, &height);
			glfwWaitEvents();
		}

		device.waitIdle();
		cleanupSwapChain();

		createSwapChain();
		createImageViews();
	}

	void cleanupSwapChain() {
		swapChainImageViews.clear();
		swapChain = nullptr;
	}

	void createSwapChain()
	{
		vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
		swapChainExtent                                = chooseSwapExtent(surfaceCapabilities);
		uint32_t minImageCount                         = chooseSwapMinImageCount(surfaceCapabilities);

		std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR(*surface);
		swapChainSurfaceFormat                             = chooseSwapSurfaceFormat(availableFormats);

		std::vector<vk::PresentModeKHR> availablePresentModes = physicalDevice.getSurfacePresentModesKHR(*surface);
		vk::PresentModeKHR              presentMode           = chooseSwapPresentMode(availablePresentModes);

		vk::SwapchainCreateInfoKHR swapChainCreateInfo{.surface          = *surface,
													   .minImageCount    = minImageCount,
													   .imageFormat      = swapChainSurfaceFormat.format,
													   .imageColorSpace  = swapChainSurfaceFormat.colorSpace,
													   .imageExtent      = swapChainExtent,
													   .imageArrayLayers = 1,
													   .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment,
													   .imageSharingMode = vk::SharingMode::eExclusive,
													   .preTransform     = surfaceCapabilities.currentTransform,
													   .compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque,
													   .presentMode      = presentMode,
													   .clipped          = true};

		swapChain       = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
		swapChainImages = swapChain.getImages();
	}

	void createImageViews()
	{
		assert(swapChainImageViews.empty());

		swapChainImageViews.reserve(swapChainImages.size());

		for (auto &image : swapChainImages)
		{
			swapChainImageViews.emplace_back(createImageView(image, swapChainSurfaceFormat.format));
		}
	}

	void createDescriptorSetLayout() {
		vk::DescriptorSetLayoutBinding uboLayoutBinding{
			.binding = 0,
			.descriptorType = vk::DescriptorType::eUniformBuffer,
			.descriptorCount = 1,
			.stageFlags = vk::ShaderStageFlagBits::eVertex
		};

		vk::DescriptorSetLayoutBinding imageSamplerBinding{
			.binding = 1,
			.descriptorType = vk::DescriptorType::eCombinedImageSampler,
			.descriptorCount = 1,
			.stageFlags = vk::ShaderStageFlagBits::eFragment
		};

		std::array<vk::DescriptorSetLayoutBinding, 2> bindings{
			uboLayoutBinding,
			imageSamplerBinding
		};

		vk::DescriptorSetLayoutCreateInfo layoutInfo{
			.bindingCount = bindings.size(),
			.pBindings = bindings.data()
		};

		descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
	}

	void createGraphicsPipeline()
	{
		vk::raii::ShaderModule shaderModule = createShaderModule(readFile("shaders/slang.spv"));

		vk::PipelineShaderStageCreateInfo vertShaderStageInfo{.stage = vk::ShaderStageFlagBits::eVertex, .module = shaderModule, .pName = "vertMain"};
		vk::PipelineShaderStageCreateInfo fragShaderStageInfo{.stage = vk::ShaderStageFlagBits::eFragment, .module = shaderModule, .pName = "fragMain"};
		vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};


		auto bindingDescription = Vertex::getBindingDescription();
		auto attributeDescriptions = Vertex::getAttributeDescriptions();
		vk::PipelineVertexInputStateCreateInfo vertexInputInfo {
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &bindingDescription,
			.vertexAttributeDescriptionCount = attributeDescriptions.size(),
			.pVertexAttributeDescriptions = attributeDescriptions.data()
		};

		vk::PipelineInputAssemblyStateCreateInfo inputAssembly{.topology = vk::PrimitiveTopology::eTriangleList};

		std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
		vk::PipelineDynamicStateCreateInfo dynamicState{.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()), .pDynamicStates = dynamicStates.data()};

		vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1, .scissorCount = 1};

		vk::PipelineRasterizationStateCreateInfo rasterizer{.depthClampEnable        = vk::False,
															.rasterizerDiscardEnable = vk::False,
															.polygonMode             = vk::PolygonMode::eFill,
															.cullMode                = vk::CullModeFlagBits::eBack,
															.frontFace               = vk::FrontFace::eCounterClockwise,
															.depthBiasEnable         = vk::False,
															.lineWidth               = 1.0f};

		vk::PipelineMultisampleStateCreateInfo multisampling{.rasterizationSamples = vk::SampleCountFlagBits::e1, .sampleShadingEnable = vk::False};

		vk::PipelineColorBlendAttachmentState colorBlendAttachment{
			.blendEnable         = vk::True,
			.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
			.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
			.colorBlendOp        = vk::BlendOp::eAdd,
			.srcAlphaBlendFactor = vk::BlendFactor::eOne,
			.dstAlphaBlendFactor = vk::BlendFactor::eZero,
			.alphaBlendOp        = vk::BlendOp::eAdd,
			.colorWriteMask      = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};

		vk::PipelineColorBlendStateCreateInfo colorBlending{
			.logicOpEnable = vk::False, .logicOp = vk::LogicOp::eCopy, .attachmentCount = 1, .pAttachments = &colorBlendAttachment};

		vk::PipelineLayoutCreateInfo pipelineLayoutInfo{.setLayoutCount = 1, .pSetLayouts = &*descriptorSetLayout, .pushConstantRangeCount = 0};
		pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

		vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
			{.stageCount          = 2,
			 .pStages             = shaderStages,
			 .pVertexInputState   = &vertexInputInfo,
			 .pInputAssemblyState = &inputAssembly,
			 .pViewportState      = &viewportState,
			 .pRasterizationState = &rasterizer,
			 .pMultisampleState   = &multisampling,
			 .pColorBlendState    = &colorBlending,
			 .pDynamicState       = &dynamicState,
			 .layout              = pipelineLayout,
			 .renderPass          = nullptr},
			{.colorAttachmentCount = 1, .pColorAttachmentFormats = &swapChainSurfaceFormat.format}};

		graphicsPipeline = vk::raii::Pipeline(device, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
	}

	void createCommandPool() {
		vk::CommandPoolCreateInfo poolInfo{
			.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
			.queueFamilyIndex = graphicsQueueFamilyIndex
		};

		commandPool = vk::raii::CommandPool(device, poolInfo);
	}

	void createCommandBuffer() {
		vk::CommandBufferAllocateInfo allocInfo {
			.commandPool = commandPool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = MAX_FRAMES_IN_FLIGHT
		};

		commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
	}

	std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) {
		vk::BufferCreateInfo bufferInfo{
			.size = size,
			.usage = usage,
			.sharingMode = vk::SharingMode::eExclusive
		};

		auto buffer = vk::raii::Buffer(device, bufferInfo);

		vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
		vk::MemoryAllocateInfo memoryAllocateInfo{
			.allocationSize = memRequirements.size,
			.memoryTypeIndex = findMemoryType(
					memRequirements.memoryTypeBits,
					vk::MemoryPropertyFlagBits::eHostVisible |
					vk::MemoryPropertyFlagBits::eHostCoherent
				)
		};

		auto bufferMemory = vk::raii::DeviceMemory(device, memoryAllocateInfo);
		buffer.bindMemory(*bufferMemory, 0);
		return {std::move(buffer), std::move(bufferMemory)};
	}
	vk::raii::CommandBuffer beginSingleTimeCommands()
	{
		vk::CommandBufferAllocateInfo allocInfo{.commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1};
		vk::raii::CommandBuffer       commandBuffer = std::move(vk::raii::CommandBuffers(device, allocInfo).front());

		vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
		commandBuffer.begin(beginInfo);

		return std::move(commandBuffer);
	}

	void endSingleTimeCommands(vk::raii::CommandBuffer &&commandBuffer)
	{
		commandBuffer.end();

		vk::SubmitInfo submitInfo{.commandBufferCount = 1, .pCommandBuffers = &*commandBuffer};
		queue.submit(submitInfo, nullptr);
		queue.waitIdle();
	}

	void copyBuffer(vk::raii::Buffer &srcBuffer, vk::raii::Buffer &dstBuffer, vk::DeviceSize size)
	{
		vk::raii::CommandBuffer commandCopyBuffer = beginSingleTimeCommands();
		commandCopyBuffer.copyBuffer(*srcBuffer, *dstBuffer, vk::BufferCopy{.size = size});
		endSingleTimeCommands(std::move(commandCopyBuffer));
	}

	std::pair<vk::raii::Image, vk::raii::DeviceMemory> createImage(
		uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties
	) {
		vk::ImageCreateInfo imageInfo{
			.imageType = vk::ImageType::e2D,
			.format = format,
			.extent = {width,height,1},
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = vk::SampleCountFlagBits::e1,
			.tiling = tiling,
			.usage = usage,
			.sharingMode = vk::SharingMode::eExclusive
		};

		vk::raii::Image image = vk::raii::Image(device, imageInfo);

		vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
		vk::MemoryAllocateInfo allocateInfo{
			.allocationSize = memRequirements.size,
			.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)
		};

		vk::raii::DeviceMemory imageMemory = vk::raii::DeviceMemory(device, allocateInfo);

		image.bindMemory(imageMemory, 0);
		return {std::move(image), std::move(imageMemory)};

	}

	void copyBufferToImage(vk::raii::CommandBuffer & commandBuffer, vk::raii::Buffer &buffer, vk::raii::Image & image, uint32_t width, uint32_t height) {
		vk::BufferImageCopy region{
			.bufferOffset = 0,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.mipLevel = 0,
				.baseArrayLayer = 0,
				.layerCount = 1
			},
			.imageOffset = {0,0,0},
			.imageExtent = {width, height, 1}
		};

		commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);
	}

	void createTextureImage() {
		int texWidth, texHeight, texChannels;
		stbi_uc * pixels = stbi_load(
			"textures/texture.jpg",
			&texWidth,
			&texHeight,
			&texChannels,
			STBI_rgb_alpha
			);

		vk::DeviceSize imageSize = texWidth * texHeight * 4;

		if (!pixels) {
			throw std::runtime_error("failed to load texture image!");
		}

		auto [stagingBuffer, stagingBufferMemory] =
			createBuffer(
				imageSize,
				vk::BufferUsageFlagBits::eTransferSrc,
				vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
				);

		void* data = stagingBufferMemory.mapMemory(0, imageSize);
		memcpy(data, pixels, imageSize);
		stagingBufferMemory.unmapMemory();
		stbi_image_free(pixels);

		std::tie(textureImage, textureImageMemory) =
			createImage(
				texWidth,
				texHeight,
				vk::Format::eR8G8B8A8Srgb,
				vk::ImageTiling::eOptimal,
				vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
				vk::MemoryPropertyFlagBits::eDeviceLocal
				);

		vk::raii::CommandBuffer commandBuffer = beginSingleTimeCommands();
		transitionImageLayout(commandBuffer, textureImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
		copyBufferToImage(commandBuffer, stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
		transitionImageLayout(commandBuffer, textureImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
		endSingleTimeCommands(std::move(commandBuffer));

	}

	vk::raii::ImageView createImageView(vk::Image const & image, vk::Format format) {
		vk::ImageViewCreateInfo viewInfo{
			.image = image,
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			}
		};

		return vk::raii::ImageView(device, viewInfo);
	}

	void createTextureImageView() {
		textureImageView = createImageView(textureImage, vk::Format::eR8G8B8A8Srgb);
	}

	void createTextureSampler() {
		vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();
		vk::SamplerCreateInfo samplerInfo{
			.magFilter    = vk::Filter::eLinear,
			.minFilter    = vk::Filter::eLinear,
			.mipmapMode   = vk::SamplerMipmapMode::eLinear,
			.addressModeU = vk::SamplerAddressMode::eRepeat,
			.addressModeV = vk::SamplerAddressMode::eRepeat,
			.addressModeW = vk::SamplerAddressMode::eRepeat,
			.mipLodBias = 0.0f,
			.anisotropyEnable = vk::True,
			.maxAnisotropy = properties.limits.maxSamplerAnisotropy,
			.compareEnable = vk::False,
			.compareOp = vk::CompareOp::eAlways,
			.minLod = 0.0f,
			.maxLod = 1.0f,
			.borderColor = vk::BorderColor::eIntOpaqueBlack,
			.unnormalizedCoordinates = vk::False
		};
		textureSampler = vk::raii::Sampler(device, samplerInfo);
	}

	void createVertexBuffer() {
		using bufferFlags = vk::BufferUsageFlagBits;
		using memoryFlags = vk::MemoryPropertyFlagBits;
		const size_t size = vertices.size() * sizeof(vertices[0]);

		auto [vBuffer, vBufferMemory] =
			createBuffer(
				size,
				bufferFlags::eTransferSrc,
				memoryFlags::eHostVisible | memoryFlags::eHostCoherent
				);


		void* data = vBufferMemory.mapMemory(0, size);
		memcpy(data, vertices.data(), size);
		vBufferMemory.unmapMemory();

		std::tie(vertexBuffer, vertexBufferMemory) =
			createBuffer(
				size,
				bufferFlags::eVertexBuffer | bufferFlags::eTransferDst,
				memoryFlags::eDeviceLocal
				);

		copyBuffer(vBuffer, vertexBuffer, size);
	}

	void createIndexBuffer() {
		using bufferFlags = vk::BufferUsageFlagBits;
		using memoryFlags = vk::MemoryPropertyFlagBits;
		vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

		auto [stagingBuffer, stagingBufferMemory] =
			createBuffer(
				bufferSize,
				bufferFlags::eTransferSrc,
				memoryFlags::eHostVisible | memoryFlags::eHostCoherent
				);

		void * data = stagingBufferMemory.mapMemory(0,bufferSize);
		memcpy(data, indices.data(), bufferSize);
		stagingBufferMemory.unmapMemory();

		std::tie(indexBuffer, indexBufferMemory) =
			createBuffer(
				bufferSize,
				bufferFlags::eIndexBuffer | bufferFlags::eTransferDst,
				memoryFlags::eDeviceLocal
			);

		copyBuffer(stagingBuffer, indexBuffer, bufferSize);
	}

	void createUniformBuffer() {
		using bufferFlags = vk::BufferUsageFlagBits;
		using memoryFlags = vk::MemoryPropertyFlagBits;

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			constexpr vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
			auto [buffer, bufferMem] =
				createBuffer(
					bufferSize,
					bufferFlags::eUniformBuffer,
					memoryFlags::eHostVisible | memoryFlags::eHostVisible
					);

			uniformBuffers.emplace_back(std::move(buffer));
			uniformBuffersMemory.emplace_back(std::move(bufferMem));
			uniformBuffersMapped.emplace_back(uniformBuffersMemory.back().mapMemory(0, bufferSize));
		}
	}

	void updateUniformBuffer(uint32_t currentFrame)
	{
		static auto startTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		float time       = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		UniformBufferObject ubo{
			.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0,0,1.0f)),
			.view = lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
			.proj = glm::perspective(glm::radians(45.0f), static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height), 0.1f, 10.0f)
		};

		//bullshit we do to  convert openGL math to vulkan standards
		ubo.proj[1][1] *= -1;

		memcpy(uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));
	}

	void createDescriptorPool() {
		vk::DescriptorPoolSize uboPoolSize{
			.type = vk::DescriptorType::eUniformBuffer,
			.descriptorCount = MAX_FRAMES_IN_FLIGHT
		};

		vk::DescriptorPoolSize imageSamplerPoolSize{
			.type = vk::DescriptorType::eCombinedImageSampler,
			.descriptorCount = MAX_FRAMES_IN_FLIGHT
		};

		std::array<vk::DescriptorPoolSize, 2> poolSizes{
			uboPoolSize,
			imageSamplerPoolSize
		};

		vk::DescriptorPoolCreateInfo poolInfo{
			.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
			.maxSets = MAX_FRAMES_IN_FLIGHT,
			.poolSizeCount = poolSizes.size(),
			.pPoolSizes = poolSizes.data()
		};

		descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
	}

	void createDescriptorSets() {
		std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);
		vk::DescriptorSetAllocateInfo        allocInfo{
			.descriptorPool     = descriptorPool,
			.descriptorSetCount = static_cast<uint32_t>(layouts.size()),
			.pSetLayouts        = layouts.data()
		};

		descriptorSets = device.allocateDescriptorSets(allocInfo);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			vk::DescriptorBufferInfo bufferInfo{
				.buffer = uniformBuffers[i],
				.offset = 0,
				.range = sizeof(UniformBufferObject)
			};

			vk::DescriptorImageInfo imageInfo{
				.sampler = textureSampler,
				.imageView = textureImageView,
				.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
			};

			vk::WriteDescriptorSet uboDescriptorWrite{
				.dstSet = descriptorSets[i],
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.pBufferInfo = &bufferInfo
			};

			vk::WriteDescriptorSet imageDescriptorWrite{
				.dstSet = descriptorSets[i],
				.dstBinding = 1,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eCombinedImageSampler,
				.pImageInfo = &imageInfo
			};

			std::array <vk::WriteDescriptorSet, 2> descriptorWrites{
				uboDescriptorWrite,
				imageDescriptorWrite
			};

			device.updateDescriptorSets(descriptorWrites, {});
		}

	}

	uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
		vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
		{
			if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}

		throw std::runtime_error("failed to find suitable memory type!");
	}

	void recordCommandBuffer(uint32_t imageIndex)
	{
		commandBuffers[frameIndex].begin({});

		transitionImageLayout(
			imageIndex,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal,
			{},                                                        // srcAccessMask (no need to wait for previous operations)
			vk::AccessFlagBits2::eColorAttachmentWrite,                // dstAccessMask
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // srcStage
			vk::PipelineStageFlagBits2::eColorAttachmentOutput         // dstStage
		);

		vk::ClearValue              clearColor     = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
		vk::RenderingAttachmentInfo attachmentInfo = {
			.imageView   = swapChainImageViews[imageIndex],
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			//wipe out the previous image clear it
			.loadOp      = vk::AttachmentLoadOp::eClear,
			//please store this and don't wipe it immediately
			.storeOp     = vk::AttachmentStoreOp::eStore,
			//clear it with this color
			.clearValue  = clearColor
		};

		vk::RenderingInfo renderingInfo = {
			.renderArea           = {
				.offset = {0, 0}, .extent = swapChainExtent
			},
			.layerCount           = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments    = &attachmentInfo
		};

		commandBuffers[frameIndex].beginRendering(renderingInfo);
		commandBuffers[frameIndex].setViewport(0,
			vk::Viewport(
				0.0f,
				0.0f,
				static_cast<float>(swapChainExtent.width),
				static_cast<float>(swapChainExtent.height),
				0.0f,
				1.0f
				)
			);

		commandBuffers[frameIndex].setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));

		commandBuffers[frameIndex].bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
		commandBuffers[frameIndex].bindVertexBuffers(0, *vertexBuffer, {0});
		commandBuffers[frameIndex].bindIndexBuffer(indexBuffer, 0, vk::IndexType::eUint16);
		commandBuffers[frameIndex].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, *descriptorSets[frameIndex], nullptr);

		commandBuffers[frameIndex].drawIndexed(
			static_cast<uint32_t>(indices.size()),
			1,
			0,
			0,
			0
			);

		//commandBuffers[frameIndex].draw(static_cast<uint32_t>(vertices.size()), 1, 0, 0);

		commandBuffers[frameIndex].endRendering();

		transitionImageLayout(
			imageIndex,
			vk::ImageLayout::eColorAttachmentOptimal,
			vk::ImageLayout::ePresentSrcKHR,
			vk::AccessFlagBits2::eColorAttachmentWrite,             // srcAccessMask
			{},                                                     // dstAccessMask
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,     // srcStage
			vk::PipelineStageFlagBits2::eBottomOfPipe               // dstStage
		);



		commandBuffers[frameIndex].end();
	}

	void transitionImageLayout(vk::raii::CommandBuffer &commandBuffer, const vk::raii::Image &image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout)
	{
		vk::ImageMemoryBarrier barrier{
			.oldLayout           = oldLayout,
			.newLayout           = newLayout,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image               = image,
			.subresourceRange    = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.levelCount = 1,
				.layerCount = 1
			}
		};
		vk::PipelineStageFlags sourceStage;
		vk::PipelineStageFlags destinationStage;

		if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
		{
			barrier.srcAccessMask = {};
			barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

			sourceStage      = vk::PipelineStageFlagBits::eTopOfPipe;
			destinationStage = vk::PipelineStageFlagBits::eTransfer;
		}
		else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
		{
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

			sourceStage      = vk::PipelineStageFlagBits::eTransfer;
			destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
		}
		else
		{
			throw std::invalid_argument("unsupported layout transition!");
		}
		commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, nullptr, barrier);
	}

	void transitionImageLayout(
		uint32_t                imageIndex,
		vk::ImageLayout         old_layout,
		vk::ImageLayout         new_layout,
		vk::AccessFlags2        src_access_mask,
		vk::AccessFlags2        dst_access_mask,
		vk::PipelineStageFlags2 src_stage_mask,
		vk::PipelineStageFlags2 dst_stage_mask)
	{
		vk::ImageMemoryBarrier2 barrier = {
			.srcStageMask        = src_stage_mask,
			.srcAccessMask       = src_access_mask,
			.dstStageMask        = dst_stage_mask,
			.dstAccessMask       = dst_access_mask,
			.oldLayout           = old_layout,
			.newLayout           = new_layout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image               = swapChainImages[imageIndex],
			.subresourceRange    =
			{
				.aspectMask     = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel   = 0,
				.levelCount     = 1,
				.baseArrayLayer = 0,
				.layerCount     = 1
			}
		};
		vk::DependencyInfo dependencyInfo = {
			.dependencyFlags         = {},
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers    = &barrier
		};
		commandBuffers[frameIndex].pipelineBarrier2(dependencyInfo);
	}

	void createSyncObjects()
	{
		assert(presentCompleteSemaphores.empty() && renderFinishedSemaphores.empty() && inFlightFences.empty());

		for (size_t i = 0; i < swapChainImages.size(); i++)
		{
			renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
		}

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			presentCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
			inFlightFences.emplace_back(device, vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
		}
	}

	void drawFrame()
	{
		auto fenceResult = device.waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);

		if (fenceResult != vk::Result::eSuccess){
			throw std::runtime_error("failed to wait for fence!");
		}

		auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphores[frameIndex], nullptr);

		if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || framebufferResized) {
			framebufferResized = false;
			recreateSwapChain();
			return;
		}

		if (result != vk::Result::eSuccess){
			assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		device.resetFences(*inFlightFences[frameIndex]);

		recordCommandBuffer(imageIndex);
		updateUniformBuffer(frameIndex);

		vk::PipelineStageFlags waitDestinationStageMask( vk::PipelineStageFlagBits::eColorAttachmentOutput );
		const vk::SubmitInfo   submitInfo{
			.waitSemaphoreCount   = 1,
			.pWaitSemaphores      = &*presentCompleteSemaphores[frameIndex],
			.pWaitDstStageMask    = &waitDestinationStageMask,
			.commandBufferCount   = 1,
			.pCommandBuffers      = &*commandBuffers[frameIndex],
			.signalSemaphoreCount = 1,
			.pSignalSemaphores    = &*renderFinishedSemaphores[imageIndex]
		};

		queue.submit(submitInfo, *inFlightFences[frameIndex]);

		frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;

		const vk::PresentInfoKHR presentInfoKHR{
			.waitSemaphoreCount = 1,
			.pWaitSemaphores    = &*renderFinishedSemaphores[imageIndex],
			.swapchainCount     = 1,
			.pSwapchains        = &*swapChain,
			.pImageIndices      = &imageIndex,
			.pResults			= nullptr
		};

		result = queue.presentKHR(presentInfoKHR);
		if ((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR)){
			recreateSwapChain();
		}
		else{
			if (result != vk::Result::eSuccess) {
				throw std::runtime_error("image failed to load");
			}
		}


	}

	[[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char> &code) const
	{
		vk::ShaderModuleCreateInfo createInfo{.codeSize = code.size() * sizeof(char), .pCode = reinterpret_cast<const uint32_t *>(code.data())};
		vk::raii::ShaderModule     shaderModule{device, createInfo};

		return shaderModule;
	}

	static uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities)
	{
		auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
		if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount))
		{
			minImageCount = surfaceCapabilities.maxImageCount;
		}
		return minImageCount;
	}

	static vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats)
	{
		assert(!availableFormats.empty());
		const auto formatIt = std::ranges::find_if(
		    availableFormats,
		    [](const auto &format) { return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear; });
		return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
	}

	static vk::PresentModeKHR chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const &availablePresentModes)
	{
		assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) { return presentMode == vk::PresentModeKHR::eFifo; }));
		return std::ranges::any_of(availablePresentModes,
		                           [](const vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value; }) ?
		           vk::PresentModeKHR::eMailbox :
		           vk::PresentModeKHR::eFifo;
	}

	vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities)
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		return {
		    std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
		    std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)};
	}

	std::vector<const char *> getRequiredInstanceExtensions()
	{
		uint32_t glfwExtensionCount = 0;
		auto     glfwExtensions     = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
		if (enableValidationLayers)
		{
			extensions.push_back(vk::EXTDebugUtilsExtensionName);
		}

		return extensions;
	}

	static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData, void *)
	{
		if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError || severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
		{
			std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;
		}

		return vk::False;
	}

	static std::vector<char> readFile(const std::string &filename)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (!file.is_open())
		{
			throw std::runtime_error("failed to open file!");
		}
		std::vector<char> buffer(file.tellg());
		file.seekg(0, std::ios::beg);
		file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
		file.close();
		return buffer;
	}
};

int main()
{
	try
	{
		HelloTriangleApplication app;
		app.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
