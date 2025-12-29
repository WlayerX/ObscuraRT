// ObscuraRT - Real-Time Video Anonymization
// Copyright (C) 2025 Semih Taha Aksoy (WlayerX)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License v3.0.

#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <cstdint>

/**
 * @brief Encapsulates Vulkan instance, device, and queue management.
 *
 * Responsibilities:
 * - Instance creation with validation layers
 * - Physical device selection
 * - Logical device + queue creation
 * - Memory management utilities
 */
class VulkanContext {
public:
    VulkanContext();
    ~VulkanContext();
    
    // Non-copyable
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
    
    // Initialization
    void init();
    void cleanup();
    
    // Getters
    VkInstance getInstance() const { return instance_; }
    VkPhysicalDevice getPhysicalDevice() const { return physical_device_; }
    VkDevice getDevice() const { return device_; }
    VkQueue getComputeQueue() const { return compute_queue_; }
    VkQueue getPresentQueue() const { return present_queue_; }
    uint32_t getComputeQueueFamily() const { return compute_queue_family_; }
    uint32_t getPresentQueueFamily() const { return present_queue_family_; }
    VkCommandPool getCommandPool() const { return command_pool_; }
    
    // Memory allocation
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    
private:
    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue compute_queue_ = VK_NULL_HANDLE;
    VkQueue present_queue_ = VK_NULL_HANDLE;
    uint32_t compute_queue_family_ = UINT32_MAX;
    uint32_t present_queue_family_ = UINT32_MAX;
    VkCommandPool command_pool_ = VK_NULL_HANDLE;
    
    // Validation
    VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;
    
    // Initialization helpers
    void createInstance();
    void setupDebugMessenger();
    void selectPhysicalDevice();
    void createLogicalDevice();
    void createCommandPool();
    std::vector<const char*> getRequiredExtensions();
    bool isDeviceSuitable(VkPhysicalDevice device);
};
