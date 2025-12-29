// ObscuraRT - Real-Time Video Anonymization
// Copyright (C) 2025 Semih Taha Aksoy (WlayerX)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License v3.0.

#include "display_pipeline.h"
#include "vulkan_context.h"
#include <iostream>
#include <vector>
#include <stdexcept>
#include <array>

DisplayPipeline::DisplayPipeline(VulkanContext* vk_ctx)
    : vk_ctx_(vk_ctx) {
}

DisplayPipeline::~DisplayPipeline() {
    cleanup();
}

void DisplayPipeline::init(uint32_t width, uint32_t height, const char* window_title) {
    createWindow(width, height, window_title);
    createSurface();
    createSwapchain(width, height);
    createImageViews();
    createRenderPass();
    createFramebuffers();
    createGraphicsPipeline();
    createCommandPool();
    allocateCommandBuffers();
    createSyncObjects();
    
    std::cout << "[Display] Pipeline initialized (" << width << "x" << height << ")" << std::endl;
}

void DisplayPipeline::cleanup() {
    VkDevice device = vk_ctx_->getDevice();
    
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
        
        if (in_flight_fence_ != VK_NULL_HANDLE) {
            vkDestroyFence(device, in_flight_fence_, nullptr);
        }
        if (render_finished_semaphore_ != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, render_finished_semaphore_, nullptr);
        }
        if (image_available_semaphore_ != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, image_available_semaphore_, nullptr);
        }
        if (command_pool_ != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, command_pool_, nullptr);
        }
        for (auto fb : framebuffers_) {
            vkDestroyFramebuffer(device, fb, nullptr);
        }
        if (render_pass_ != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device, render_pass_, nullptr);
        }
        for (auto view : swapchain_image_views_) {
            vkDestroyImageView(device, view, nullptr);
        }
        if (graphics_pipeline_ != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, graphics_pipeline_, nullptr);
        }
        if (pipeline_layout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, pipeline_layout_, nullptr);
        }
        if (swapchain_ != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device, swapchain_, nullptr);
        }
    }
    
    VkInstance instance = vk_ctx_->getInstance();
    if (instance != VK_NULL_HANDLE && surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, surface_, nullptr);
    }
    
    if (window_) {
        glfwDestroyWindow(window_);
        glfwTerminate();
    }
}

void DisplayPipeline::createWindow(uint32_t width, uint32_t height, const char* title) {
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    
    window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }
    
    std::cout << "[Display] GLFW window created (" << width << "x" << height << ")" << std::endl;
}

void DisplayPipeline::createSurface() {
    // Try to create surface from GLFW window
    // In headless environments, this might fail - handle gracefully
    VkResult result = glfwCreateWindowSurface(vk_ctx_->getInstance(), window_, nullptr, &surface_);
    
    if (result == VK_ERROR_EXTENSION_NOT_PRESENT) {
        std::cout << "[Display] WARNING: GLFW surface extension not available (headless mode)" << std::endl;
        // Continue with VK_NULL_HANDLE - will use display without actual window
        surface_ = VK_NULL_HANDLE;
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface");
    }
}

void DisplayPipeline::createSwapchain(uint32_t width, uint32_t height) {
    if (surface_ == VK_NULL_HANDLE) {
        // Headless mode: just create compatible images
        swapchain_extent_ = {width, height};
        swapchain_format_ = VK_FORMAT_R8G8B8A8_UNORM;
        swapchain_images_.resize(2);  // Double buffering
        swapchain_image_views_.resize(2);
        framebuffers_.resize(2);
        
        std::cout << "[Display] Swapchain in headless mode (2 images)" << std::endl;
        return;
    }
    
    VkPhysicalDevice physicalDevice = vk_ctx_->getPhysicalDevice();
    VkDevice device = vk_ctx_->getDevice();
    
    // Query surface capabilities
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface_, &capabilities);
    
    // Query surface formats
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface_, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface_, &formatCount, formats.data());
    
    // Choose format
    VkSurfaceFormatKHR selectedFormat = formats[0];
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_R8G8B8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            selectedFormat = format;
            break;
        }
    }
    swapchain_format_ = selectedFormat.format;
    
    // Query present modes
    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface_, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface_, &presentModeCount, presentModes.data());
    
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;  // vsync
    for (const auto& mode : presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            presentMode = mode;
            break;
        }
    }
    
    // Extent
    swapchain_extent_ = capabilities.currentExtent;
    if (capabilities.currentExtent.width == UINT32_MAX) {
        swapchain_extent_.width = width;
        swapchain_extent_.height = height;
    }
    
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }
    
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface_;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = swapchain_format_;
    createInfo.imageColorSpace = selectedFormat.colorSpace;
    createInfo.imageExtent = swapchain_extent_;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;
    
    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swapchain");
    }
    
    // Get swapchain images
    uint32_t swapchainImageCount = 0;
    vkGetSwapchainImagesKHR(device, swapchain_, &swapchainImageCount, nullptr);
    swapchain_images_.resize(swapchainImageCount);
    vkGetSwapchainImagesKHR(device, swapchain_, &swapchainImageCount, swapchain_images_.data());
    
    std::cout << "[Display] Swapchain created (" << swapchainImageCount << " images)" << std::endl;
}

void DisplayPipeline::createImageViews() {
    swapchain_image_views_.resize(swapchain_images_.size());
    
    for (size_t i = 0; i < swapchain_images_.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapchain_images_[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapchain_format_;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        
        if (vkCreateImageView(vk_ctx_->getDevice(), &createInfo, nullptr, &swapchain_image_views_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image view");
        }
    }
}

void DisplayPipeline::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchain_format_;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;
    
    if (vkCreateRenderPass(vk_ctx_->getDevice(), &renderPassInfo, nullptr, &render_pass_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass");
    }
}

void DisplayPipeline::createFramebuffers() {
    framebuffers_.resize(swapchain_image_views_.size());
    
    for (size_t i = 0; i < swapchain_image_views_.size(); i++) {
        VkImageView attachments[] = {swapchain_image_views_[i]};
        
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = render_pass_;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapchain_extent_.width;
        framebufferInfo.height = swapchain_extent_.height;
        framebufferInfo.layers = 1;
        
        if (vkCreateFramebuffer(vk_ctx_->getDevice(), &framebufferInfo, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer");
        }
    }
}

void DisplayPipeline::createGraphicsPipeline() {
    // Minimal pipeline: just clear to color (no actual drawing yet)
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    
    if (vkCreatePipelineLayout(vk_ctx_->getDevice(), &pipelineLayoutInfo, nullptr, &pipeline_layout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }
}

void DisplayPipeline::createCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = vk_ctx_->getPresentQueueFamily();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    
    if (vkCreateCommandPool(vk_ctx_->getDevice(), &poolInfo, nullptr, &command_pool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }
}

void DisplayPipeline::allocateCommandBuffers() {
    command_buffers_.resize(framebuffers_.size());
    
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = command_pool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)command_buffers_.size();
    
    if (vkAllocateCommandBuffers(vk_ctx_->getDevice(), &allocInfo, command_buffers_.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers");
    }
}

void DisplayPipeline::createSyncObjects() {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
    if (vkCreateSemaphore(vk_ctx_->getDevice(), &semaphoreInfo, nullptr, &image_available_semaphore_) != VK_SUCCESS ||
        vkCreateSemaphore(vk_ctx_->getDevice(), &semaphoreInfo, nullptr, &render_finished_semaphore_) != VK_SUCCESS ||
        vkCreateFence(vk_ctx_->getDevice(), &fenceInfo, nullptr, &in_flight_fence_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create synchronization primitives");
    }
}

bool DisplayPipeline::presentFrame(VkImage compute_output) {
    if (shouldClose()) {
        return false;
    }
    
    glfwPollEvents();
    
    VkDevice device = vk_ctx_->getDevice();
    
    // Wait for previous frame
    vkWaitForFences(device, 1, &in_flight_fence_, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &in_flight_fence_);
    
    // Acquire swapchain image
    uint32_t imageIndex;
    vkAcquireNextImageKHR(device, swapchain_, UINT64_MAX, image_available_semaphore_, VK_NULL_HANDLE, &imageIndex);
    
    // Record command buffer
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    vkBeginCommandBuffer(command_buffers_[imageIndex], &beginInfo);
    
    // Copy compute output to swapchain image
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = swapchain_images_[imageIndex];
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    
    vkCmdPipelineBarrier(command_buffers_[imageIndex], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    // Copy from compute output to swapchain
    VkImageCopy copyRegion{};
    copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.srcSubresource.mipLevel = 0;
    copyRegion.srcSubresource.baseArrayLayer = 0;
    copyRegion.srcSubresource.layerCount = 1;
    copyRegion.srcOffset = {0, 0, 0};
    copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.dstSubresource.mipLevel = 0;
    copyRegion.dstSubresource.baseArrayLayer = 0;
    copyRegion.dstSubresource.layerCount = 1;
    copyRegion.dstOffset = {0, 0, 0};
    copyRegion.extent.width = swapchain_extent_.width;
    copyRegion.extent.height = swapchain_extent_.height;
    copyRegion.extent.depth = 1;
    
    vkCmdCopyImage(command_buffers_[imageIndex], compute_output, VK_IMAGE_LAYOUT_GENERAL, swapchain_images_[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
    
    // Transition to present layout
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = 0;
    
    vkCmdPipelineBarrier(command_buffers_[imageIndex], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    vkEndCommandBuffer(command_buffers_[imageIndex]);
    
    // Submit
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &image_available_semaphore_;
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &command_buffers_[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &render_finished_semaphore_;
    
    if (vkQueueSubmit(vk_ctx_->getPresentQueue(), 1, &submitInfo, in_flight_fence_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit queue");
    }
    
    // Present
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &render_finished_semaphore_;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain_;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;
    
    vkQueuePresentKHR(vk_ctx_->getPresentQueue(), &presentInfo);
    
    return true;
}

bool DisplayPipeline::shouldClose() const {
    return window_ && glfwWindowShouldClose(window_);
}
