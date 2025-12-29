// ObscuraRT - Real-Time Video Anonymization
// Copyright (C) 2025 Semih Taha Aksoy (WlayerX)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License v3.0.

#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <cstdint>
#include <memory>
#include <vector>

class VulkanContext;

/**
 * @brief Renders processed frames to display via Vulkan + GLFW.
 *
 * Pipeline:
 * 1. Acquire swapchain image
 * 2. Copy compute output to swapchain
 * 3. Present to screen
 * 4. Optional: fps counter
 */
class DisplayPipeline {
public:
    explicit DisplayPipeline(VulkanContext* vk_ctx);
    ~DisplayPipeline();
    
    DisplayPipeline(const DisplayPipeline&) = delete;
    DisplayPipeline& operator=(const DisplayPipeline&) = delete;
    
    void init(uint32_t width, uint32_t height, const char* window_title = "ObscuraRT");
    void cleanup();
    
    /**
     * @brief Present frame to display.
     * @param frame_image VkImage with final pixelated frame
     * @return true if should continue, false if window closed
     */
    bool presentFrame(VkImage frame_image);
    
    bool shouldClose() const;
    
    GLFWwindow* getWindow() const { return window_; }
    VkSurfaceKHR getSurface() const { return surface_; }
    
private:
    VulkanContext* vk_ctx_;
    GLFWwindow* window_ = nullptr;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    std::vector<VkImage> swapchain_images_;
    std::vector<VkImageView> swapchain_image_views_;
    VkFormat swapchain_format_;
    VkExtent2D swapchain_extent_;
    
    VkRenderPass render_pass_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;
    
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline graphics_pipeline_ = VK_NULL_HANDLE;
    VkCommandPool command_pool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> command_buffers_;
    
    VkSemaphore image_available_semaphore_ = VK_NULL_HANDLE;
    VkSemaphore render_finished_semaphore_ = VK_NULL_HANDLE;
    VkFence in_flight_fence_ = VK_NULL_HANDLE;
    
    uint32_t current_frame_ = 0;
    
    void createWindow(uint32_t width, uint32_t height, const char* title);
    void createSurface();
    void createSwapchain(uint32_t width, uint32_t height);
    void createImageViews();
    void createRenderPass();
    void createFramebuffers();
    void createGraphicsPipeline();
    void createCommandPool();
    void allocateCommandBuffers();
    void createSyncObjects();
};
