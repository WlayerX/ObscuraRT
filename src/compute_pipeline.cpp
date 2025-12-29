// ObscuraRT - Real-Time Video Anonymization
// Copyright (C) 2025 Semih Taha Aksoy (WlayerX)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License v3.0.

#include "compute_pipeline.h"
#include "vulkan_context.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <stdexcept>

// Helper to load SPIR-V shader
static std::vector<uint32_t> readShaderFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + filename);
    }
    
    size_t fileSize = (size_t)file.tellg();
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
    
    file.seekg(0);
    file.read((char*)buffer.data(), fileSize);
    file.close();
    
    return buffer;
}

ComputePipeline::ComputePipeline(VulkanContext* vk_ctx)
    : vk_ctx_(vk_ctx) {
}

ComputePipeline::~ComputePipeline() {
    cleanup();
}

void ComputePipeline::init(uint32_t width, uint32_t height) {
    width_ = width;
    height_ = height;
    
    createImages();
    createStagingBuffer();
    createShaderModule();
    createDescriptorSetLayout();
    createPipelineLayout();
    createComputePipeline();
    createDescriptorPool();
    allocateDescriptorSets();
    updateDescriptorSets();
    createCommandBuffer();
    createSynchronization();
    
    std::cout << "[Compute] Pipeline initialized (" << width_ << "x" << height_ << ")" << std::endl;
}

void ComputePipeline::cleanup() {
    VkDevice device = vk_ctx_->getDevice();
    
    if (compute_fence_ != VK_NULL_HANDLE) {
        vkDestroyFence(device, compute_fence_, nullptr);
    }
    if (descriptor_pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptor_pool_, nullptr);
    }
    if (compute_pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, compute_pipeline_, nullptr);
    }
    if (pipeline_layout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipeline_layout_, nullptr);
    }
    if (descriptor_set_layout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, descriptor_set_layout_, nullptr);
    }
    if (compute_shader_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, compute_shader_, nullptr);
    }
    
    if (input_image_view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device, input_image_view_, nullptr);
    }
    if (output_image_view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device, output_image_view_, nullptr);
    }
    if (input_image_ != VK_NULL_HANDLE) {
        vkDestroyImage(device, input_image_, nullptr);
    }
    if (output_image_ != VK_NULL_HANDLE) {
        vkDestroyImage(device, output_image_, nullptr);
    }
    if (input_image_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device, input_image_memory_, nullptr);
    }
    if (output_image_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device, output_image_memory_, nullptr);
    }
    
    if (staging_buffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, staging_buffer_, nullptr);
    }
    if (staging_buffer_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device, staging_buffer_memory_, nullptr);
    }
}

void ComputePipeline::createShaderModule() {
    auto code = readShaderFile("shaders/pixelation.comp.spv");
    
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size() * sizeof(uint32_t);
    createInfo.pCode = code.data();
    
    if (vkCreateShaderModule(vk_ctx_->getDevice(), &createInfo, nullptr, &compute_shader_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute shader module");
    }
}

void ComputePipeline::createDescriptorSetLayout() {
    // Two storage images: input and output
    VkDescriptorSetLayoutBinding bindings[2];
    
    // Input image
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[0].pImmutableSamplers = nullptr;
    
    // Output image
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].pImmutableSamplers = nullptr;
    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings = bindings;
    
    if (vkCreateDescriptorSetLayout(vk_ctx_->getDevice(), &layoutInfo, nullptr, &descriptor_set_layout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout");
    }
}

void ComputePipeline::createPipelineLayout() {
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptor_set_layout_;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    
    if (vkCreatePipelineLayout(vk_ctx_->getDevice(), &pipelineLayoutInfo, nullptr, &pipeline_layout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }
}

void ComputePipeline::createComputePipeline() {
    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfo.module = compute_shader_;
    shaderStageInfo.pName = "main";
    
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = pipeline_layout_;
    pipelineInfo.stage = shaderStageInfo;
    
    if (vkCreateComputePipelines(vk_ctx_->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &compute_pipeline_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute pipeline");
    }
}

void ComputePipeline::createDescriptorPool() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSize.descriptorCount = 2 * 2;  // 2 sets, 2 images each
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 2;
    
    if (vkCreateDescriptorPool(vk_ctx_->getDevice(), &poolInfo, nullptr, &descriptor_pool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool");
    }
}

void ComputePipeline::allocateDescriptorSets() {
    VkDescriptorSetLayout layouts[2] = {descriptor_set_layout_, descriptor_set_layout_};
    
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptor_pool_;
    allocInfo.descriptorSetCount = 2;
    allocInfo.pSetLayouts = layouts;
    
    if (vkAllocateDescriptorSets(vk_ctx_->getDevice(), &allocInfo, descriptor_sets_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets");
    }
}

void ComputePipeline::createCommandBuffer() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = vk_ctx_->getCommandPool();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    
    if (vkAllocateCommandBuffers(vk_ctx_->getDevice(), &allocInfo, &compute_command_buffer_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffer");
    }
}

void ComputePipeline::createSynchronization() {
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // Start signaled
    
    if (vkCreateFence(vk_ctx_->getDevice(), &fenceInfo, nullptr, &compute_fence_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create fence");
    }
}

void ComputePipeline::processFrame(VkImage input_image, VkImage output_image, uint32_t block_size) {
    // MVP stub: this will be fully implemented
    // For now, just signal that compute is done
}

VkDescriptorSet ComputePipeline::getDescriptorSet(uint32_t frame_index) const {
    return descriptor_sets_[frame_index % 2];
}

void ComputePipeline::createImages() {
    VkDevice device = vk_ctx_->getDevice();
    VkPhysicalDevice physicalDevice = vk_ctx_->getPhysicalDevice();
    
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent.width = width_;
    imageInfo.extent.height = height_;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_LINEAR;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    
    if (vkCreateImage(device, &imageInfo, nullptr, &input_image_) != VK_SUCCESS ||
        vkCreateImage(device, &imageInfo, nullptr, &output_image_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create images");
    }
    
    // Allocate memory
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, input_image_, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = vk_ctx_->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    if (vkAllocateMemory(device, &allocInfo, nullptr, &input_image_memory_) != VK_SUCCESS ||
        vkAllocateMemory(device, &allocInfo, nullptr, &output_image_memory_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate image memory");
    }
    
    vkBindImageMemory(device, input_image_, input_image_memory_, 0);
    vkBindImageMemory(device, output_image_, output_image_memory_, 0);
    
    // Create image views
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    viewInfo.image = input_image_;
    if (vkCreateImageView(device, &viewInfo, nullptr, &input_image_view_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create input image view");
    }
    
    viewInfo.image = output_image_;
    if (vkCreateImageView(device, &viewInfo, nullptr, &output_image_view_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create output image view");
    }
}

void ComputePipeline::createStagingBuffer() {
    VkDevice device = vk_ctx_->getDevice();
    
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = width_ * height_ * 4;  // RGBA
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(device, &bufferInfo, nullptr, &staging_buffer_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create staging buffer");
    }
    
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, staging_buffer_, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = vk_ctx_->findMemoryType(memRequirements.memoryTypeBits, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    if (vkAllocateMemory(device, &allocInfo, nullptr, &staging_buffer_memory_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate staging buffer memory");
    }
    
    vkBindBufferMemory(device, staging_buffer_, staging_buffer_memory_, 0);
}

void ComputePipeline::updateDescriptorSets() {
    VkDevice device = vk_ctx_->getDevice();
    
    for (int i = 0; i < 2; i++) {
        VkDescriptorImageInfo inputImageInfo{};
        inputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        inputImageInfo.imageView = input_image_view_;
        
        VkDescriptorImageInfo outputImageInfo{};
        outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        outputImageInfo.imageView = output_image_view_;
        
        VkWriteDescriptorSet descriptorWrites[2]{};
        
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptor_sets_[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo = &inputImageInfo;
        
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptor_sets_[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &outputImageInfo;
        
        vkUpdateDescriptorSets(device, 2, descriptorWrites, 0, nullptr);
    }
}
