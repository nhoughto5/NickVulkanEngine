#include "nvrpch.h"
#include "SwapChain.h"

SwapChain::SwapChain(GLFWwindow* window, LogicalDevice *logicalDevice, PhysicalDevice *physicalDevice, Surface* surface)
	:
	m_Window(window),
	m_PhysicalDevice(physicalDevice),
	m_LogicalDevice(logicalDevice),
	m_Surface(surface)
{
	SwapChainSupportDetails swapChainSupport = m_PhysicalDevice->querySwapChainSupport(m_PhysicalDevice->getPhysicalDevice());
	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
	VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

	// Choose number of images in swap chain
	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = m_Surface->getSurface();
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; //Render directly to images

	// How swap chain images will be used across multiple queue families
	QueueFamilyIndices indices = m_PhysicalDevice->findQueueFamilies(m_PhysicalDevice->getPhysicalDevice());
	uint32_t queueFamilyIndices[] = { (uint32_t)indices.graphicsFamily, (uint32_t)indices.presentFamily };

	if (indices.graphicsFamily != indices.presentFamily) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0; // Optional
		createInfo.pQueueFamilyIndices = nullptr; // Optional
	}

	createInfo.preTransform = swapChainSupport.capabilities.currentTransform; //Transforms can happen in the swap chain
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE; // Don't care about pixels not currently visible
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(m_LogicalDevice->getLogicalDevice(), &createInfo, nullptr, &m_SwapChain) != VK_SUCCESS) {
		throw std::runtime_error("failed to create swap chain!");
	}

	vkGetSwapchainImagesKHR(m_LogicalDevice->getLogicalDevice(), m_SwapChain, &imageCount, nullptr);
	m_SwapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(m_LogicalDevice->getLogicalDevice(), m_SwapChain, &imageCount, m_SwapChainImages.data());
	m_SwapChainImageFormat = surfaceFormat.format;
	m_SwapChainExtent = extent;

	createImageViews();
}

void SwapChain::createFrameBuffers(VkRenderPass renderPass, VkImageView colorImageView, VkImageView depthImageView) {
	m_SwapChainFrameBuffers.resize(m_SwapChainImageViews.size());

	for (size_t i = 0; i < m_SwapChainImageViews.size(); ++i) {
		std::array<VkImageView, 3> attachments = {
			colorImageView,
			depthImageView,
			m_SwapChainImageViews[i]
		};

		VkFramebufferCreateInfo frameBufferInfo = {};
		frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameBufferInfo.renderPass = renderPass;
		frameBufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		frameBufferInfo.pAttachments = attachments.data();
		frameBufferInfo.width = m_SwapChainExtent.width;
		frameBufferInfo.height = m_SwapChainExtent.height;
		frameBufferInfo.layers = 1;

		if (vkCreateFramebuffer(m_LogicalDevice->getLogicalDevice(), &frameBufferInfo, nullptr, &m_SwapChainFrameBuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create framebuffer");
		}
	}
}

// Choosing the resolution of Images in swap chain
VkExtent2D SwapChain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	}
	else {
		int width, height;
		glfwGetWindowSize(m_Window, &width, &height);
		VkExtent2D actualExtent = { width, height };

		actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
		actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

		return actualExtent;
	}
}

// Choose the conditions which dictate the swapping of rendered images to the screen
VkPresentModeKHR SwapChain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes) {
	// Guranteed to be available: "Images submitted by your application are transferred to the screen right away, which may result in tearing"
	VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;

	for (const auto& availablePresentMode : availablePresentModes) {
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			return availablePresentMode;
		}
		else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
			bestMode = availablePresentMode;
		}
	}

	return bestMode;
}

// Setting the color depth
VkSurfaceFormatKHR SwapChain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
	// Ideal: can choose any color format
	if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) {
		return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	}

	// If can not choose anything see what is available.
	for (const auto& availableFormat : availableFormats) {
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return availableFormat;
		}
	}
}

void SwapChain::createImageViews() {
	m_SwapChainImageViews.resize(m_SwapChainImages.size());

	for (uint32_t i = 0; i < m_SwapChainImages.size(); i++) {
		m_SwapChainImageViews[i] = createImageView(m_SwapChainImages[i], m_SwapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, m_LogicalDevice->getLogicalDevice());
	}
}

void SwapChain::Cleanup() {
	for (size_t i = 0; i < m_SwapChainImageViews.size(); i++) {
		vkDestroyImageView(m_LogicalDevice->getLogicalDevice(), m_SwapChainImageViews[i], nullptr);
	}

	vkDestroySwapchainKHR(m_LogicalDevice->getLogicalDevice(), m_SwapChain, nullptr);
}

SwapChain::~SwapChain()
{
	Cleanup();
}

VkFormat SwapChain::getSwapChainImageFormat()
{
	return m_SwapChainImageFormat;
}

VkExtent2D SwapChain::getSwapChainExtent()
{
	return m_SwapChainExtent;
}

const std::vector<VkImage>& SwapChain::getSwapChainImages() const
{
	return m_SwapChainImages;
}

const std::vector<VkImageView>& SwapChain::getSwapChainImageViews() const
{
	return m_SwapChainImageViews;
}

const std::vector<VkFramebuffer>& SwapChain::getSwapChainFrameBuffers() const
{
	return m_SwapChainFrameBuffers;
}

const VkFramebuffer SwapChain::getFrameBufferByIndex(int i) const
{
	return m_SwapChainFrameBuffers[i];
}

VkSwapchainKHR SwapChain::getSwapChain()
{
	return m_SwapChain;
}