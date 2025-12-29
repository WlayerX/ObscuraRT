# ObscuraRT Quick Reference

## Command Cheat Sheet

### Build

```bash
# Full rebuild
./setup.sh  # Auto-download dependencies
cd build && make -j$(nproc)

# Quick rebuild (after code change)
cd build && make

# Release build (optimized)
cmake -DCMAKE_BUILD_TYPE=Release .. && make

# Debug build (symbols + validation)
cmake -DCMAKE_BUILD_TYPE=Debug .. && make
```

### Run

```bash
# Basic
./build/obscura_rt

# With validation layers
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation ./build/obscura_rt

# Verbose output
./build/obscura_rt 2>&1 | tee run.log
```

### Shader Compilation

```bash
# Manual (usually automatic via CMake)
glslc shaders/pixelation.comp -o build/shaders/pixelation.comp.spv

# Check syntax
glslangValidator shaders/pixelation.comp
```

### Profiling

```bash
# GPU timeline (requires RenderDoc)
renderdoc ./build/obscura_rt

# CPU profile
perf record -g ./build/obscura_rt
perf report

# Memory usage
valgrind --tool=massif ./build/obscura_rt
```

---

## Code Structure Quick Map

| File | Purpose |
|------|---------|
| `src/main.cpp` | Entry point, main loop |
| `src/vulkan_context.cpp` | Vulkan setup (instance, device, queues) |
| `src/frame_grabber.cpp` | Video input (webcam, file) |
| `src/compute_pipeline.cpp` | GPU compute pipeline |
| `src/display_pipeline.cpp` | Window + presentation (stub) |
| `include/*.h` | Public interfaces |
| `shaders/pixelation.comp` | Main pixelation shader |
| `shaders/display.{vert,frag}` | Graphics shaders (stub) |

---

## Key Classes & Methods

### VulkanContext

```cpp
vk_ctx_->init();                            // Initialize Vulkan
vk_ctx_->getDevice();                       // VkDevice handle
vk_ctx_->getComputeQueue();                 // Compute queue
vk_ctx_->findMemoryType(typeFilter, props); // Find memory type
```

### FrameGrabber

```cpp
grabber_->init();                           // Open webcam/file
grabber_->grabFrame(frame);                 // Capture frame
frame.width, frame.height, frame.data;      // Frame properties
```

### ComputePipeline

```cpp
pipeline_->init(1920, 1080);                // Initialize
pipeline_->processFrame(in, out, blockSize); // Run shader
```

### DisplayPipeline

```cpp
display_->init(1920, 1080, "window title"); // Create window
display_->presentFrame(image);              // Show frame
display_->shouldClose();                    // Check exit
```

---

## Troubleshooting

### "Vulkan API not available"

```bash
# Check GPU drivers
vulkaninfo | head -20

# Install Vulkan tools
sudo apt-get install vulkan-tools vulkan-headers libvulkan-dev
```

### "glslc not found"

```bash
# Install Vulkan SDK
wget https://vulkan.lunarg.com/sdk/home.txt
# Or via package manager
sudo apt-get install glslang-tools
```

### "Failed to find GPU"

```bash
# Check connected GPUs
lspci | grep -i vga
vulkaninfo | grep "GPU" -A5
```

### "Shader compilation error"

```bash
# Validate syntax
glslangValidator shaders/pixelation.comp

# Verbose compilation
glslc -v shaders/pixelation.comp
```

### "Memory allocation failure"

- Reduce resolution (1080p → 720p)
- Enable memory compression (GPU driver setting)
- Profile memory usage: `vulkaninfo --summary`

---

## Architecture at a Glance

```
User Input (Camera)
    ↓
FrameGrabber (CPU)
    ↓ [RGBA data]
GPU Upload (VkBuffer → VkImage)
    ↓
ComputePipeline (Pixelation Shader)
    ↓
GPU Download (VkImage → VkBuffer)
    ↓
DisplayPipeline (GLFW Window)
    ↓
Screen Output
```

---

## Performance Targets

- **Resolution**: 1920×1080
- **Frame Rate**: 30–60 FPS
- **Latency**: <15 ms per frame
- **GPU Memory**: <500 MB
- **CPU Usage**: <20%

---

## File Locations

```
Project root:      /home/wlayerx/Development/Projects/ObscuraRT/
Source code:       ./src/*.cpp
Headers:           ./include/*.h
Shaders (GLSL):    ./shaders/*.{comp,vert,frag}
Built executable:  ./build/obscura_rt
Shader binaries:   ./build/shaders/*.spv
```

---

## Important Vulkan Concepts

| Term | Meaning |
|------|---------|
| **VkInstance** | Vulkan application context |
| **VkPhysicalDevice** | GPU hardware |
| **VkDevice** | Logical GPU interface |
| **VkQueue** | Command submission point |
| **VkCommandBuffer** | Commands (upload, compute, render) |
| **VkImage** | GPU texture/framebuffer |
| **VkBuffer** | GPU memory block |
| **VkPipeline** | Compute/graphics state machine |
| **VkShaderModule** | Compiled SPIR-V shader |

---

## Next Steps

1. **Run setup.sh** → Downloads GLM, configures CMake
2. **Build** → `make -j$(nproc)` in `build/`
3. **Test MVP** → `./obscura_rt` should show pixelated test pattern
4. **Understand the code** → Read [ARCHITECTURE.md](ARCHITECTURE.md)
5. **Extend** → Follow [ROADMAP.md](ROADMAP.md) phases

---

## Resources

- [Vulkan Spec](https://registry.khronos.org/vulkan/)
- [Khronos Samples](https://github.com/KhronosGroup/Vulkan-Samples)
- [GLSL Spec](https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.4.60.pdf)
- [GLM Docs](https://glm.g-truc.net/)

