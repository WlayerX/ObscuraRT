// ObscuraRT - Real-Time Video Anonymization
// Copyright (C) 2025 Semih Taha Aksoy (WlayerX)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License v3.0.

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <memory>
#include <glm/glm.hpp>

class VulkanContext;

/**
 * @brief GPU-accelerated pixelation via Vulkan compute.
 *
 * Pipeline:
 * 1. Input: RGBA image (YUV420 if needed, convert in CPU first)
 * 2. Compute shader:
 *    - Read input image
 *    - Apply block-based pixelation
 * 3. Output: Pixelated RGBA image
 *
 * For MVP: entire frame pixelation. Later: selective (face region only).
 */
class ComputePipeline {
public:
    explicit ComputePipeline(VulkanContext* vk_ctx);
    ~ComputePipeline();
    
    ComputePipeline(const ComputePipeline&) = delete;
    ComputePipeline& operator=(const ComputePipeline&) = delete;
    
    void init(uint32_t width, uint32_t height);
    void cleanup();
    
    /**
     * @brief Execute pixelation compute shader.
     * @param input_image VkImage containing input frame (RGBA)
     * @param output_image VkImage for output (RGBA)
     * @param block_size Pixelation block size in pixels (e.g., 16)
     */
    void processFrame(VkImage input_image, VkImage output_image, uint32_t block_size = 16);
    
    VkImage getOutputImage() const { return output_image_; }
    
    // Memory accessors for main loop
    VkDeviceMemory getStagingBufferMemory() const { return staging_buffer_memory_; }
    size_t getStagingBufferSize() const { return width_ * height_ * 4; }
    VkImage getInputImage() const { return input_image_; }
    
    // Accessors
    VkDescriptorSet getDescriptorSet(uint32_t frame_index) const;
    
private:
    VulkanContext* vk_ctx_;
    uint32_t width_;
    uint32_t height_;
    
    VkShaderModule compute_shader_ = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline compute_pipeline_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptor_set_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
    
    // For double-buffering: 2 descriptor sets
    VkDescriptorSet descriptor_sets_[2];
    
    VkCommandBuffer compute_command_buffer_ = VK_NULL_HANDLE;
    VkFence compute_fence_ = VK_NULL_HANDLE;
    
    VkImage input_image_ = VK_NULL_HANDLE;
    VkImage output_image_ = VK_NULL_HANDLE;
    VkDeviceMemory input_image_memory_ = VK_NULL_HANDLE;
    VkDeviceMemory output_image_memory_ = VK_NULL_HANDLE;
    VkImageView input_image_view_ = VK_NULL_HANDLE;
    VkImageView output_image_view_ = VK_NULL_HANDLE;
    
    VkBuffer staging_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory staging_buffer_memory_ = VK_NULL_HANDLE;
    
    void createShaderModule();
    void createPipelineLayout();
    void createComputePipeline();
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void allocateDescriptorSets();
    void createCommandBuffer();
    void createSynchronization();
    void createImages();
    void createStagingBuffer();
    void updateDescriptorSets();
};
