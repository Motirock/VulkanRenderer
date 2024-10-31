/*
3-D Voxel-Based Terrain Generator
By Victor Monnier
2024
*/

/* Quick Use
glslc shaders/composition.vert -o shaders/composition.vert.spv && glslc shaders/composition.frag -o shaders/composition.frag.spv && glslc shaders/main.vert -o shaders/main.vert.spv && glslc shaders/main.frag -o shaders/main.frag.spv && glslc shaders/light.vert -o shaders/light.vert.spv && glslc shaders/light.frag -o shaders/light.frag.spv && glslc shaders/skybox.vert -o shaders/skybox.vert.spv && glslc shaders/skybox.frag -o shaders/skybox.frag.spv && glslc shaders/bloom.comp -o shaders/bloom.comp.spv && g++ -c -std=c++17 -O3 src/*.cxx -Iinclude && g++ *.o -o bin/main -Llib -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi && ./bin/main
*/

/* NO DEBUG
g++ -c -std=c++17 -DNDEBUG -O3 src/*.cxx -Iinclude 
g++ objects/*.o -o bin/main -Llib -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi
./bin/main
g++ -c -std=c++17 -DNDEBUG -O3 src/*.cxx -Iinclude && g++ *.o -o bin/main -Llib -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi && ./bin/main
*/
/* DEBUG
g++ -c -std=c++17 -O3 src/*.cxx -Iinclude
g++ *.o -o bin/main -Llib -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi
./bin/main
g++ -c -std=c++17 -O3 src/*.cxx -Iinclude && g++ *.o -o bin/main -Llib -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi && ./bin/main
*/
/* SHADER COMPILATION
glslc shaders/main.vert -o shaders/main.vert.spv
glslc shaders/main.frag -o shaders/main.frag.spv
glslc shaders/light.vert -o shaders/light.vert.spv
glslc shaders/light.frag -o shaders/light.frag.spv
glslc shaders/ocean.vert -o shaders/ocean.vert.spv
glslc shaders/ocean.frag -o shaders/ocean.frag.spv
glslc shaders/skybox.vert -o shaders/skybox.vert.spv
glslc shaders/skybox.frag -o shaders/skybox.frag.spv
glslc shaders/composition.vert -o shaders/composition.vert.spv
glslc shaders/composition.frag -o shaders/composition.frag.spv
glslc shaders/bloom.comp -o shaders/bloom.comp.spv
*/

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <vector>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <array>
#include <optional>
#include <set>
#include <thread>
#include <unordered_map>
#include <string>

#include "MarchingCubes.h"
#include "PerlinNoise.hpp"

using namespace VkUtils;

const uint32_t WIDTH = 1600;
const uint32_t HEIGHT = 900;

#ifdef DYNAMIC_BLOOM_RESOLUTION
const float BLOOM_RATIO = 0.5f;
#endif
#ifndef DYNAMIC_BLOOM_RESOLUTION
const uint32_t BLOOM_WIDTH = 400;
const uint32_t BLOOM_HEIGHT = 225;
#endif

const uint32_t TEXTURE_COUNT = 9;
const std::string SKYBOX_PATH = "textures/skybox/";
const std::string TEXTURE_PATHS[TEXTURE_COUNT] = {
    SKYBOX_PATH+"front.jpg",
    SKYBOX_PATH+"back.jpg",
    SKYBOX_PATH+"left.jpg",
    SKYBOX_PATH+"right.jpg",
    SKYBOX_PATH+"top.jpg",
    SKYBOX_PATH+"bottom.jpg",
    "textures/dusty_albedo.png",
    "textures/dusty_normal.png",
    "textures/dusty_roughness.png",
};

const uint32_t MAX_POLYGON_COUNT = 30000;

const int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks *pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsAndComputeFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsAndComputeFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct UniformBufferObject {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 projection;
    alignas(16) glm::mat4 reflectView;
    alignas(16) glm::mat4 reflectProjection;
    alignas(16) glm::mat4 refractView;
    alignas(16) glm::mat4 refractProjection;
    alignas(16) glm::vec3 cameraPosition;
    alignas(16) glm::vec3 viewDirection;
    float nearPlane;
    float farPlane;
    float time;
    float gamma;
    float exposure;
};

struct PolygonInfo {
    alignas(16) glm::vec3 tangent;
    alignas(16) glm::vec3 bitangent;
    alignas(16) glm::vec3 normal;
};

struct PolygonInfoBufferObject {
    PolygonInfo polygonInfos[MAX_POLYGON_COUNT];  
};

struct Light {
    alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 color;
    float strength; //!= 0
};

const uint32_t MAX_LIGHTS = 2000;
struct LightBufferObject {
    int32_t lightCount;
    alignas(16) Light lights[MAX_LIGHTS];
};

class VulkanTerrainApplication {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow *window;

    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;

    VkQueue graphicsQueue;
    VkQueue computeQueue;
    VkQueue presentQueue;

    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    VkExtent2D bloomImageExtent;

    std::vector<VkFramebuffer> gBuffers;

    VkRenderPass offScreenRenderPass;
    FrameBufferAttachment positionAttachment;
    FrameBufferAttachment colorAttachment;
    FrameBufferAttachment normalAttachment;
    FrameBufferAttachment bloomAttachment;
    FrameBufferAttachment depthAttachment;
    static const uint32_t COLOR_ATTACHMENT_COUNT = 4;

    VkImage bloomImages[2];
    VkDeviceMemory bloomImageMemories[2];
    VkImageView bloomImageViews[2];

    VkRenderPass onScreenRenderPass;

    VkDescriptorSetLayout offScreenDescriptorSetLayout;
    VkDescriptorSetLayout computeDescriptorSetLayout;
    VkDescriptorSetLayout onScreenDescriptorSetLayout;

    VkPipelineLayout mainPipelineLayout;
    VkPipeline mainPipeline;

    VkPipelineLayout lightPipelineLayout;
    VkPipeline lightPipeline;

    VkPipelineLayout skyboxPipelineLayout;
    VkPipeline skyboxPipeline;

    VkPipelineLayout bloomPipelineLayout;
    VkPipeline bloomPipeline;

    VkPipelineLayout compositionPipelineLayout;
    VkPipeline compositionPipeline;

    VkCommandPool commandPool;

    Texture textures[TEXTURE_COUNT];
    
    VkSampler textureSampler;
    VkSampler skyboxSampler;
    VkSampler compositionSampler;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    void *stagingBufferMapped;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<PolygonInfo> polygonInfos;
    PolygonInfoBufferObject polygonInfoBufferObject;
    VkBuffer vertexBuffer;
    VkBuffer indexBuffer;
    VkBuffer polygonInfoBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkDeviceMemory indexBufferMemory;
    VkDeviceMemory polygonInfoBufferMemory;
    void *vertexData;
    void *indexData;
    void *polygonInfoBufferMapped;

    bool isLightsChanged = false;
    std::vector<LightVertex> lightVertices;
    std::vector<uint32_t> lightIndices;
    VkBuffer lightVertexBuffer;
    VkBuffer lightIndexBuffer;
    VkDeviceMemory lightVertexBufferMemory;
    VkDeviceMemory lightIndexBufferMemory;
    void *lightVertexBufferMapped;
    void *lightIndexBufferMapped;

    VkBuffer skyboxVertexBuffer;
    VkDeviceMemory skyboxVertexBufferMemory;

    VkBuffer compositionVertexBuffer;
    VkDeviceMemory compositionVertexBufferMemory;

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

    std::vector<Light> lights;
    const float LIGHT_SIZE = 1.0f;
    bool isLightBufferChanged = false;
    bool isDay = true;

    VkBuffer lightBuffer;
    VkDeviceMemory lightBufferMemory;
    void *lightBufferMapped;

    const uint32_t WORLD_X = 80;
    const uint32_t WORLD_Y = 80;
    const uint32_t WORLD_Z = 10;
    const float SEA_LEVEL = WORLD_Z/2.0f;

    VkDescriptorPool offScreenDescriptorPool;
    VkDescriptorPool computeDescriptorPool;
    VkDescriptorPool onScreenDescriptorPool;
    std::vector<VkDescriptorSet> offScreenDescriptorSets;
    std::vector<VkDescriptorSet> computeDescriptorSets;
    std::vector<VkDescriptorSet> onScreenDescriptorSets;

    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkCommandBuffer> computeCommandBuffers;
    std::vector<VkCommandBuffer> offScreenCommandBuffers;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> offScreenRenderFinishedSemaphores;
    std::vector<VkSemaphore> computeFinishedSemaphores;
    std::vector<VkSemaphore> onScreenRenderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> computeInFlightFences;
    std::vector<VkFence> deferredInFlightFences;
    uint32_t currentFrame = 0;

    float time;

    bool framebufferResized = false;
    uint32_t TPS = 60, ticks = 0;

    glm::vec3 cameraPosition = glm::vec3(10.0f, 10.0f, 35.0f);
    glm::vec3 viewDirection = glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f));
    //Degrees, xy plane, xz plane
    glm::vec2 viewAngles = glm::vec2(0, 0);
    float FOV = 90.0f;

    float nearPlane = 0.1f;
    float farPlane = 10000.0f;

    float exposure = 0.01f;

    float speed = 10.0f;
    float turningSensitivity = 0.2f;

    void initWindow() {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Renderer", nullptr, nullptr);
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
        glfwSetMouseButtonCallback(window, mouseButtonCallback);
        glfwSetCursorPosCallback(window, cursorPosCallback);
        glfwSetCursorEnterCallback(window, cursorEnterCallback);
        glfwSetScrollCallback(window, scrollCallback);
    }

    static void framebufferResizeCallback(GLFWwindow *window, int width, int height) {
        auto app = reinterpret_cast<VulkanTerrainApplication*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
    }

    static void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods) {
        auto app = reinterpret_cast<VulkanTerrainApplication*>(glfwGetWindowUserPointer(window));
        if(button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
            double xPos, yPos;
            glfwGetCursorPos(window, &xPos, &yPos);
            app->mouseX = xPos/app->swapChainExtent.width;
            app->mouseY = yPos/app->swapChainExtent.height;
            app->mouseLeftPressed = true;
        }
        if(button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
            double xPos, yPos;
            glfwGetCursorPos(window, &xPos, &yPos);
            app->mouseX = xPos/app->swapChainExtent.width;
            app->mouseY = yPos/app->swapChainExtent.height;
            app->mouseLeftPressed = false;
            app->mouseLeftClicked = true;
        }
        if(button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
            double xPos, yPos;
            glfwGetCursorPos(window, &xPos, &yPos);
            app->mouseX = xPos/app->swapChainExtent.width;
            app->mouseY = yPos/app->swapChainExtent.height;
            app->mouseRightPressed = true;
        }
        if(button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
            double xPos, yPos;
            glfwGetCursorPos(window, &xPos, &yPos);
            app->mouseX = xPos/app->swapChainExtent.width;
            app->mouseY = yPos/app->swapChainExtent.height;
            app->mouseRightPressed = false;
            app->mouseRightClicked = true;
        }
        app->cursorJustEntered = false;
        //std::cout << "Cursor Position at (" << (float) app->mouseX << " : " << (float) app->mouseX << ") " << app->mouseLeftPressed << "\n";
    }

    static void cursorPosCallback(GLFWwindow *window, double xPos, double yPos) {
        auto app = reinterpret_cast<VulkanTerrainApplication*>(glfwGetWindowUserPointer(window));
        if (!app->cursorJustEntered) {
            app->mouseX = xPos/app->swapChainExtent.width;
            app->mouseY = yPos/app->swapChainExtent.height;
        }
        app->cursorJustEntered = false;
        //std::cout << "Cursor Position at (" << (float) app->mouseX << " : " << (float) app->mouseX << ") " << app->mouseLeftPressed << "\n";
    }

    static void cursorEnterCallback(GLFWwindow *window, int entered) {
        auto app = reinterpret_cast<VulkanTerrainApplication*>(glfwGetWindowUserPointer(window));
        app->cursorJustEntered = true;
    }

    static void scrollCallback(GLFWwindow *window, double xOffset, double yOffset) {
        auto app = reinterpret_cast<VulkanTerrainApplication*>(glfwGetWindowUserPointer(window));
        app->scrollAmount = (float) yOffset;
    }

    void initVulkan() {
        createInstance();

        setupDebugMessenger();

        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();

        createSwapChain();
        createImageViews();

        createOffScreenFrameBufferAttachments();

        createOffScreenRenderPass();
        createOnScreenRenderPass();

        createOffScreenDescriptorSetLayout();
        createComputeDescriptorSetLayout();
        createOnScreenDescriptorSetLayout();

        createMainPipeline();
        createLightPipeline();
        createSkyboxPipeline();
        createBloomPipeline();
        createCompositionPipeline();

        createCommandPool();

        createFramebuffers();

        createBloomImages();

        createTextureImages();
        createTextureImageViews();

        createTextureSampler();
        createSkyboxSampler();
        createCompositionSampler();

        buildWorld();

        createLightVertexBuffer();
        createLightIndexBuffer();
        createSkyboxVertexBuffer();
        createCompositionVertexBuffer();

        createUniformBuffers();
        createLightBuffer();

        createOffScreenDescriptorPool();
        createComputeDescriptorPool();
        createOnScreenDescriptorPool();
        createOffScreenDescriptorSets();
        createComputeDescriptorSets();
        createOnScreenDescriptorSets();

        createCommandBuffers();
        createComputeCommandBuffers();

        createSyncObjects();
    }

    float mouseX = 0.0f, mouseY = 0.0f;
    float previousMouseX = mouseX, previousMouseY = mouseY;
    bool mouseLeftPressed = false, mouseRightPressed = false;
    bool mouseLeftClicked = false, mouseRightClicked = false;
    float scrollAmount = 0;
    bool lockedIn = false;
    bool cursorJustEntered = false;
    bool xPressed = false, zPressed = false, cPressed = false, bPressed = false;
    void input() {
        float speed = this->speed;
        float turningSensitivity = this->turningSensitivity*FOV/90.0f;
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
            speed = this->speed/5.0f;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            cameraPosition += viewDirection*speed*(1.0f/TPS);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            cameraPosition -= viewDirection*speed*(1.0f/TPS);
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            cameraPosition.z += speed*(1.0f/TPS);
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            cameraPosition.z -= speed*(1.0f/TPS);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            cameraPosition += glm::normalize(glm::vec3(
                std::cos(glm::radians(-viewAngles.x+90))*std::cos(glm::radians(viewAngles.y)), 
                std::sin(glm::radians(-viewAngles.x+90))*std::cos(glm::radians(viewAngles.y)), 
                0))*speed*(1.0f/TPS);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            cameraPosition += glm::normalize(glm::vec3(
                std::cos(glm::radians(-viewAngles.x-90))*std::cos(glm::radians(viewAngles.y)), 
                std::sin(glm::radians(-viewAngles.x-90))*std::cos(glm::radians(viewAngles.y)), 
                0))*speed*(1.0f/TPS);

        //Mouseless turning
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            viewAngles.y += turningSensitivity;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            viewAngles.y -= turningSensitivity;
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            viewAngles.x -= turningSensitivity;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            viewAngles.x += turningSensitivity;

        if (!xPressed && glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS && lights.size() < MAX_LIGHTS) {
            float r = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
            float g = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
            float b = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);

            lights.push_back(Light{
                glm::vec3(cameraPosition.x, cameraPosition.y, cameraPosition.z),
                glm::vec3(r, g, b),
                5.0f
            });

            isLightsChanged = true;
        }
        xPressed = glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS;

        if (!zPressed && glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS && lights.size() > 0) {
            lights.pop_back();
            isLightsChanged = true;
        }
        zPressed = glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS;;

        if (!cPressed && glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
            lights.clear();
            isLightsChanged = true;
        }
        cPressed = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;

        bPressed = glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS;

        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
           exposure *= 1.01f;
        }
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
            exposure /= 1.01f;
        }

        bool lockingIn = !lockedIn && glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS;
        bool unlocking = lockedIn && glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
        lockedIn = glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS;

        if (lockingIn)
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        if (lockedIn && !lockingIn) {
            viewAngles.x += (mouseX-previousMouseX)*turningSensitivity*swapChainExtent.width;
            viewAngles.y -= (mouseY-previousMouseY)*turningSensitivity*swapChainExtent.height;
        }
        if (unlocking)
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

        if (scrollAmount > 0)
            FOV -= 5.0f;
        if (scrollAmount < 0)
            FOV += 5.0f;
        scrollAmount = 0;
        if (FOV < 1.0f)
            FOV = 1.0f;
        if (FOV > 90.0f)    
            FOV = 90.0f;

        glm::vec3 viewDirection = glm::normalize(glm::vec3(
            std::cos(glm::radians(-viewAngles.x))*std::cos(glm::radians(viewAngles.y)), 
            std::sin(glm::radians(-viewAngles.x))*std::cos(glm::radians(viewAngles.y)), 
            std::sin(glm::radians(viewAngles.y))
        ));

        previousMouseX = mouseX;
        previousMouseY = mouseY;

        mouseLeftClicked = false;
        mouseRightClicked = false;
    }

    void mainLoop() {
        float averageFrameTime = 0.0f;

        while (!glfwWindowShouldClose(window)) {
            static auto startTime = std::chrono::high_resolution_clock::now();
            auto currentTime = std::chrono::high_resolution_clock::now();
            time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

            glfwPollEvents();
            if (time*TPS >= ticks) {
                input();
                update();
                drawFrame();

                #ifdef SHOW_POSITION
                std::cout << "X Y Z: " << cameraPosition.x << " " << cameraPosition.y << " " << cameraPosition.z << "\n";
                #endif
                #ifndef HIDE_DIAGNOSTICS
                auto currentTime = std::chrono::high_resolution_clock::now();
                float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count()-time;
                averageFrameTime += frameTime/TPS;
                
                if (ticks % TPS == 0) {
                    uint32_t vertexCount = vertices.size();
                    uint32_t indexCount = indices.size()+lightIndices.size()+36;
                    
                    std::cout << "Frame time: " << averageFrameTime <<
                        ", estimated maximum FPS: " << (int) (1.0f/averageFrameTime) << "\n"
                        << "Polygons rendered: " << vertexCount/3 << "\n"
                        // << "Vertex count: " << vertexCount << " Vertex memory size: " << vertexCount*sizeof(Vertex) << "\n"
                        // << "Index count: " << indexCount << " Index memory size: " << indexCount*sizeof(uint32_t) << "\n"
                        << "\n";

                    averageFrameTime = 0.0f;
                }
                #endif
                ticks++;
            }
        }

        vkDeviceWaitIdle(device);
    }
    
    void cleanupSwapChain() {
        for (uint32_t i = 0; i < 2; i++) {
            vkDestroyImageView(device, bloomImageViews[i], nullptr);
            vkDestroyImage(device, bloomImages[i], nullptr);
            vkFreeMemory(device, bloomImageMemories[i], nullptr);
        }

        for (auto framebuffer : swapChainFramebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }

        for (auto framebuffer : gBuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }

        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }

        

        vkDestroySwapchainKHR(device, swapChain, nullptr);
    }

    void cleanup() {
        std::cout << "\n\nPROGRAM TERMINATED. CLEANING UP.\n\n";

        cleanupSwapChain();

        vkDestroyPipeline(device, mainPipeline, nullptr);
        vkDestroyPipelineLayout(device, mainPipelineLayout, nullptr);

        vkDestroyPipeline(device, lightPipeline, nullptr);
        vkDestroyPipelineLayout(device, lightPipelineLayout, nullptr);

        vkDestroyPipeline(device, skyboxPipeline, nullptr);
        vkDestroyPipelineLayout(device, skyboxPipelineLayout, nullptr);

        vkDestroyPipeline(device, bloomPipeline, nullptr);
        vkDestroyPipelineLayout(device, bloomPipelineLayout, nullptr);

        vkDestroyPipeline(device, compositionPipeline, nullptr);
        vkDestroyPipelineLayout(device, compositionPipelineLayout, nullptr);

        vkDestroyRenderPass(device, offScreenRenderPass, nullptr);
        positionAttachment.cleanup(device);
        colorAttachment.cleanup(device);
        normalAttachment.cleanup(device);
        bloomAttachment.cleanup(device);
        depthAttachment.cleanup(device);

        vkDestroyRenderPass(device, onScreenRenderPass, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroyBuffer(device, uniformBuffers[i], nullptr);
            vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
        }

        vkDestroyBuffer(device, lightBuffer, nullptr);
        vkFreeMemory(device, lightBufferMemory, nullptr);

        vkDestroyDescriptorPool(device, offScreenDescriptorPool, nullptr);
        vkDestroyDescriptorPool(device, computeDescriptorPool, nullptr);
        vkDestroyDescriptorPool(device, onScreenDescriptorPool, nullptr);

        for (int i = 0; i < TEXTURE_COUNT; i++)
            textures[i].cleanup(device);

        vkDestroySampler(device, textureSampler, nullptr);
        vkDestroySampler(device, skyboxSampler, nullptr);
        vkDestroySampler(device, compositionSampler, nullptr);

        vkDestroyDescriptorSetLayout(device, offScreenDescriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, computeDescriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, onScreenDescriptorSetLayout, nullptr);

        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);

        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);

        vkDestroyBuffer(device, polygonInfoBuffer, nullptr);
        vkFreeMemory(device, polygonInfoBufferMemory, nullptr);

        vkDestroyBuffer(device, lightVertexBuffer, nullptr);
        vkFreeMemory(device, lightVertexBufferMemory, nullptr);

        vkDestroyBuffer(device, lightIndexBuffer, nullptr);
        vkFreeMemory(device, lightIndexBufferMemory, nullptr);

        vkDestroyBuffer(device, skyboxVertexBuffer, nullptr);
        vkFreeMemory(device, skyboxVertexBufferMemory, nullptr);

        vkDestroyBuffer(device, compositionVertexBuffer, nullptr);
        vkFreeMemory(device, compositionVertexBufferMemory, nullptr);

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(device, offScreenRenderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(device, computeFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(device, onScreenRenderFinishedSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);
            vkDestroyFence(device, computeInFlightFences[i], nullptr);
            vkDestroyFence(device, deferredInFlightFences[i], nullptr);
        }

        vkDestroyCommandPool(device, commandPool, nullptr);

        vkDestroyDevice(device, nullptr);

        if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }

        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);

        glfwTerminate();
    }

    void recreateSwapChain() {
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }
        vkDeviceWaitIdle(device);

        cleanupSwapChain();

        createSwapChain();

        createOffScreenFrameBufferAttachments();

        createBloomImages();

        createImageViews();
        createFramebuffers();

        recreateComputeDescriptorSets();
        recreateOnScreenDescriptorSets();
    }

    void createInstance() {
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("Validation layers requested, but not available!");
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Vulkan Renderer";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
        } else {
            createInfo.enabledLayerCount = 0;

            createInfo.pNext = nullptr;
        }

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create instance!");
        }
    }

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
    }

    void setupDebugMessenger() {
        if (!enableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("Failed to set up debug messenger!");
        }
    }

    void createSurface() {
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create window surface!");
        }
    }

    void pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (deviceCount == 0) {
            throw std::runtime_error("Failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const auto &device : devices) {
            if (isDeviceSuitable(device)) {
                physicalDevice = device;
                break;
            }
        }

        if (physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("Failed to find a suitable GPU!");
        }
    }

    void createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsAndComputeFamily.value(), indices.presentFamily.value()};

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.shaderClipDistance = VK_TRUE;

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        createInfo.pEnabledFeatures = &deviceFeatures;

        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device!");
        }

        vkGetDeviceQueue(device, indices.graphicsAndComputeFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.graphicsAndComputeFamily.value(), 0, &computeQueue);
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
    }

    void createSwapChain() {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;

        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = {indices.graphicsAndComputeFamily.value(), indices.presentFamily.value()};

        if (indices.graphicsAndComputeFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;

        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create swap chain!");
        }

        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;

        #ifdef DYNAMIC_BLOOM_RESOLUTION
        bloomImageExtent = {(uint32_t) (swapChainExtent.width*BLOOM_RATIO), (uint32_t) (swapChainExtent.height*BLOOM_RATIO)}; 
        #endif
        #ifndef DYNAMIC_BLOOM_RESOLUTION
        bloomImageExtent = {BLOOM_WIDTH, BLOOM_HEIGHT};
        #endif
    }

    void createImageViews() {
        swapChainImageViews.resize(swapChainImages.size());

        for (uint32_t i = 0; i < swapChainImages.size(); i++) {
            swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
        }
    }

    void createAttachment(VkFormat format, uint32_t usage, FrameBufferAttachment *attachment, uint32_t width, uint32_t height) {
		VkImageAspectFlags aspectMask = 0;
		VkImageLayout imageLayout;

		attachment->format = format;

		if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
			aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
		if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
			aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			if (format >= VK_FORMAT_D16_UNORM_S8_UINT)
				aspectMask |=VK_IMAGE_ASPECT_STENCIL_BIT;
			imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		assert(aspectMask > 0);
		
        createImage(swapChainExtent.width, swapChainExtent.height, 1, 1, format, VK_IMAGE_TILING_OPTIMAL, usage | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, attachment->image, attachment->memory);

		VkImageViewCreateInfo imageView = VkImageViewCreateInfo{};
        imageView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageView.format = format;
		imageView.subresourceRange = {};
		imageView.subresourceRange.aspectMask = aspectMask;
		imageView.subresourceRange.baseMipLevel = 0;
		imageView.subresourceRange.levelCount = 1;
		imageView.subresourceRange.baseArrayLayer = 0;
		imageView.subresourceRange.layerCount = 1;
		imageView.image = attachment->image;
		if (vkCreateImageView(device, &imageView, nullptr, &attachment->view) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image view for attachment!");
        }
	}

    void createBloomImages() {
        createImage(bloomImageExtent.width, bloomImageExtent.height, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, bloomImages[0], bloomImageMemories[0]);
        transitionImageLayout(bloomImages[0], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 1);
        bloomImageViews[0] = createImageView(bloomImages[0], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1);

        createImage(bloomImageExtent.width, bloomImageExtent.height, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, bloomImages[1], bloomImageMemories[1]);
        transitionImageLayout(bloomImages[1], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 1);
        bloomImageViews[1] = createImageView(bloomImages[1], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }

    void createOffScreenFrameBufferAttachments() {
        createAttachment(
			VK_FORMAT_R16G16B16A16_SFLOAT,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			&positionAttachment,
            swapChainExtent.width, swapChainExtent.height);

        createAttachment(
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			&colorAttachment,
            swapChainExtent.width, swapChainExtent.height);
            
        createAttachment(
			VK_FORMAT_R8G8B8A8_SNORM,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			&normalAttachment,
            swapChainExtent.width, swapChainExtent.height);

        createAttachment(
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			&bloomAttachment,
            swapChainExtent.width, swapChainExtent.height);    

		createAttachment(
			findDepthFormat(),
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			&depthAttachment,
            swapChainExtent.width, swapChainExtent.height);
    }

    void createOffScreenRenderPass() {
        VkAttachmentDescription attachmentDescs[COLOR_ATTACHMENT_COUNT+1];

		//Initialize attachment properties
		for (uint32_t i = 0; i < COLOR_ATTACHMENT_COUNT+1; i++) {
			attachmentDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
			attachmentDescs[i].loadOp = i == COLOR_ATTACHMENT_COUNT ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachmentDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachmentDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

			if (i == COLOR_ATTACHMENT_COUNT) {
				attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			}
            else if (i == COLOR_ATTACHMENT_COUNT-1) {
                attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }
			else {
				attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}

            attachmentDescs[i].flags = 0;
		}

		//Formats
        attachmentDescs[0].format = positionAttachment.format;
		attachmentDescs[1].format = colorAttachment.format;
        attachmentDescs[2].format = normalAttachment.format;
        attachmentDescs[3].format = bloomAttachment.format;
		attachmentDescs[4].format = depthAttachment.format;

		std::vector<VkAttachmentReference> colorReferences;
        for (uint32_t i = 0; i < COLOR_ATTACHMENT_COUNT; i++)
		    colorReferences.push_back({ i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

		VkAttachmentReference depthReference = {};
		depthReference.attachment = COLOR_ATTACHMENT_COUNT;
		depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.pColorAttachments = colorReferences.data();
		subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
		subpass.pDepthStencilAttachment = &depthReference;

		// Use subpass dependencies for attachment layout transitions
		std::array<VkSubpassDependency, 2> dependencies;

		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.pAttachments = attachmentDescs;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(COLOR_ATTACHMENT_COUNT+1);
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 2;
		renderPassInfo.pDependencies = dependencies.data();

        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &offScreenRenderPass) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create render pass!");
        }
    }

    void createOnScreenRenderPass() {
        std::array<VkAttachmentDescription, 1> attachments = {};
        //Color attachment
        attachments[0].format = swapChainImageFormat;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorReference = {};
        colorReference.attachment = 0;
        colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpassDescription = {};
        subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescription.colorAttachmentCount = 1;
        subpassDescription.pColorAttachments = &colorReference;
        subpassDescription.pDepthStencilAttachment = nullptr;
        subpassDescription.inputAttachmentCount = 0;
        subpassDescription.pInputAttachments = nullptr;
        subpassDescription.preserveAttachmentCount = 0;
        subpassDescription.pPreserveAttachments = nullptr;
        subpassDescription.pResolveAttachments = nullptr;

        // Subpass dependencies for layout transitions
        std::array<VkSubpassDependency, 1> dependencies;

        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].srcAccessMask = 0;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        dependencies[0].dependencyFlags = 0;

        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpassDescription;
        renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        renderPassInfo.pDependencies = dependencies.data();

        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &onScreenRenderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass!");
        }
    }

    void createOffScreenDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.pImmutableSamplers = nullptr;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 1;
        samplerLayoutBinding.descriptorCount = TEXTURE_COUNT;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.pImmutableSamplers = nullptr;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding polygonInfoBufferLayoutBinding{};
        polygonInfoBufferLayoutBinding.binding = 2;
        polygonInfoBufferLayoutBinding.descriptorCount = 1;
        polygonInfoBufferLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        polygonInfoBufferLayoutBinding.pImmutableSamplers = nullptr;
        polygonInfoBufferLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 3> bindings = {uboLayoutBinding, samplerLayoutBinding, polygonInfoBufferLayoutBinding};
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &offScreenDescriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor set layout!");
        }
    }

    void createComputeDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding firstBloomImageLayoutBinding{};
        firstBloomImageLayoutBinding.binding = 0;
        firstBloomImageLayoutBinding.descriptorCount = 1;
        firstBloomImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        firstBloomImageLayoutBinding.pImmutableSamplers = nullptr;
        firstBloomImageLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutBinding secondBloomImageLayoutBinding{};
        secondBloomImageLayoutBinding.binding = 1;
        secondBloomImageLayoutBinding.descriptorCount = 1;
        secondBloomImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        secondBloomImageLayoutBinding.pImmutableSamplers = nullptr;
        secondBloomImageLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {firstBloomImageLayoutBinding, secondBloomImageLayoutBinding};
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &computeDescriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor set layout!");
        }
    }

    void createOnScreenDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.pImmutableSamplers = nullptr;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 1;
        samplerLayoutBinding.descriptorCount = COLOR_ATTACHMENT_COUNT;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.pImmutableSamplers = nullptr;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding lightBufferLayoutBinding{};
        lightBufferLayoutBinding.binding = 2;
        lightBufferLayoutBinding.descriptorCount = 1;
        lightBufferLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        lightBufferLayoutBinding.pImmutableSamplers = nullptr;
        lightBufferLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 3> bindings = {uboLayoutBinding, samplerLayoutBinding, lightBufferLayoutBinding};
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &onScreenDescriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor set layout!");
        }
    }

    void createMainPipeline() {
        auto vertShaderCode = readFile("shaders/main.vert.spv");
        auto fragShaderCode = readFile("shaders/main.frag.spv");

        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        std::array<VkVertexInputBindingDescription, 1> bindingDescriptions = {Vertex::getBindingDescription()};
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = Vertex::getAttributeDescriptions();

        vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.minSampleShading = 0.0f;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState colorBlendAttachments[COLOR_ATTACHMENT_COUNT];
        for (uint32_t i = 0; i < COLOR_ATTACHMENT_COUNT; i++) {
            colorBlendAttachments[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            colorBlendAttachments[i].blendEnable = VK_TRUE;
            colorBlendAttachments[i].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachments[i].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
            colorBlendAttachments[i].colorBlendOp = VK_BLEND_OP_ADD;
            colorBlendAttachments[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachments[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            colorBlendAttachments[i].alphaBlendOp = VK_BLEND_OP_ADD;
        }

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = COLOR_ATTACHMENT_COUNT;
        colorBlending.pAttachments = colorBlendAttachments;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPushConstantRange pushConstantRanges[1];
        pushConstantRanges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRanges[0].offset = 0;
        pushConstantRanges[0].size = sizeof(uint32_t)*2;

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &offScreenDescriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges;

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &mainPipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout!");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = mainPipelineLayout;
        pipelineInfo.renderPass = offScreenRenderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mainPipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline!");
        }

        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
    }

    void createLightPipeline() {
        auto vertShaderCode = readFile("shaders/light.vert.spv");
        auto fragShaderCode = readFile("shaders/light.frag.spv");

        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        std::array<VkVertexInputBindingDescription, 1> bindingDescriptions = {LightVertex::getBindingDescription()};
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = LightVertex::getAttributeDescriptions();

        vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.minSampleShading = 0.0f;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState colorBlendAttachments[COLOR_ATTACHMENT_COUNT];
        for (uint32_t i = 0; i < COLOR_ATTACHMENT_COUNT; i++) {
            colorBlendAttachments[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            colorBlendAttachments[i].blendEnable = VK_TRUE;
            colorBlendAttachments[i].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachments[i].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
            colorBlendAttachments[i].colorBlendOp = VK_BLEND_OP_ADD;
            colorBlendAttachments[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachments[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            colorBlendAttachments[i].alphaBlendOp = VK_BLEND_OP_ADD;
        }

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = COLOR_ATTACHMENT_COUNT;
        colorBlending.pAttachments = colorBlendAttachments;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPushConstantRange pushConstantRanges[1];
        pushConstantRanges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRanges[0].offset = 0;
        pushConstantRanges[0].size = sizeof(uint32_t)*2;

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &offScreenDescriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges;

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &lightPipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout!");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = lightPipelineLayout;
        pipelineInfo.renderPass = offScreenRenderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &lightPipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline!");
        }

        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
    }

    void createSkyboxPipeline() {
        auto vertShaderCode = readFile("shaders/skybox.vert.spv");
        auto fragShaderCode = readFile("shaders/skybox.frag.spv");

        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        std::array<VkVertexInputBindingDescription, 1> bindingDescriptions = {PositionVertex::getBindingDescription()};
        std::array<VkVertexInputAttributeDescription, 1> attributeDescriptions = {PositionVertex::getAttributeDescriptions()};

        vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.minSampleShading = 0.0f;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState colorBlendAttachments[COLOR_ATTACHMENT_COUNT];
        for (uint32_t i = 0; i < COLOR_ATTACHMENT_COUNT; i++) {
            colorBlendAttachments[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            colorBlendAttachments[i].blendEnable = VK_TRUE;
            colorBlendAttachments[i].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachments[i].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
            colorBlendAttachments[i].colorBlendOp = VK_BLEND_OP_ADD;
            colorBlendAttachments[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachments[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            colorBlendAttachments[i].alphaBlendOp = VK_BLEND_OP_ADD;
        }

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = COLOR_ATTACHMENT_COUNT;
        colorBlending.pAttachments = colorBlendAttachments;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPushConstantRange pushConstantRanges[1];
        pushConstantRanges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRanges[0].offset = 0;
        pushConstantRanges[0].size = sizeof(uint32_t)*3;

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &offScreenDescriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges;

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &skyboxPipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout!");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = skyboxPipelineLayout;
        pipelineInfo.renderPass = offScreenRenderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &skyboxPipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline!");
        }

        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
    }

    void createBloomPipeline() {
        auto computeShaderCode = readFile("shaders/bloom.comp.spv");

        VkShaderModule computeShaderModule = createShaderModule(computeShaderCode);

        VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
        computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computeShaderStageInfo.module = computeShaderModule;
        computeShaderStageInfo.pName = "main";

        VkPushConstantRange pushConstantRanges[1];
        pushConstantRanges[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRanges[0].offset = 0;
        pushConstantRanges[0].size = sizeof(uint32_t)*3;

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &computeDescriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges;

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &bloomPipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute pipeline layout!");
        }

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.layout = bloomPipelineLayout;
        pipelineInfo.stage = computeShaderStageInfo;

        if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &bloomPipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute pipeline!");
        }

        vkDestroyShaderModule(device, computeShaderModule, nullptr);
    }

    void createCompositionPipeline() {
        auto vertShaderCode = readFile("shaders/composition.vert.spv");
        auto fragShaderCode = readFile("shaders/composition.frag.spv");

        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        std::array<VkVertexInputBindingDescription, 1> bindingDescriptions = {PositionVertex::getBindingDescription()};
        std::array<VkVertexInputAttributeDescription, 1> attributeDescriptions = {PositionVertex::getAttributeDescriptions()};

        vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.minSampleShading = 0.0f;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_FALSE;
        depthStencil.depthWriteEnable = VK_FALSE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPushConstantRange pushConstantRanges[1];
        pushConstantRanges[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRanges[0].offset = 0;
        pushConstantRanges[0].size = sizeof(uint32_t);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &onScreenDescriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges;

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &compositionPipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout!");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = compositionPipelineLayout;
        pipelineInfo.renderPass = onScreenRenderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &compositionPipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline!");
        }

        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
    }

    void createFramebuffers() {
        gBuffers.resize(swapChainImageViews.size());
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            const size_t ATTACHMENT_COUNT = COLOR_ATTACHMENT_COUNT+1;
            VkImageView attachments[ATTACHMENT_COUNT];
            attachments[0] = positionAttachment.view;
            attachments[1] = colorAttachment.view;
            attachments[2] = normalAttachment.view;
            attachments[3] = bloomAttachment.view;
            attachments[4] = depthAttachment.view;

            VkFramebufferCreateInfo fbufCreateInfo = {};
            fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fbufCreateInfo.pNext = NULL;
            fbufCreateInfo.renderPass = offScreenRenderPass;
            fbufCreateInfo.pAttachments = attachments;
            fbufCreateInfo.attachmentCount = static_cast<uint32_t>(ATTACHMENT_COUNT);
            fbufCreateInfo.width = swapChainExtent.width;
            fbufCreateInfo.height = swapChainExtent.height;
            fbufCreateInfo.layers = 1;
            
            if (vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &gBuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create framebuffer!");
            }
        }

        swapChainFramebuffers.resize(swapChainImageViews.size());

        for (size_t i = 0; i < swapChainImageViews.size(); i++) {

            std::array<VkImageView, 1> attachments = {
                swapChainImageViews[i],
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = onScreenRenderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = swapChainExtent.width;
            framebufferInfo.height = swapChainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create framebuffer!");
            }
        }
    }

    void createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsAndComputeFamily.value();

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics command pool!");
        }
    }

    VkFormat findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }

        throw std::runtime_error("Failed to find supported format!");
    }

    VkFormat findDepthFormat() {
        return findSupportedFormat(
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }

    bool hasStencilComponent(VkFormat format) {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

    void createTextureImages() {
        for (int i = 0; i < TEXTURE_COUNT; i++) {
            int texWidth, texHeight, texChannels;
            stbi_uc *pixels = stbi_load(TEXTURE_PATHS[i].c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
            VkDeviceSize imageSize = texWidth*texHeight*4;
            textures[i].mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight))))+1;
            
            if (!pixels) {
                throw std::runtime_error(TEXTURE_PATHS[i]+" failed to load!");
            }

            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
            createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

            void *data;
            vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
                memcpy(data, pixels, static_cast<size_t>(imageSize));
            vkUnmapMemory(device, stagingBufferMemory);

            stbi_image_free(pixels);

            createImage(texWidth, texHeight, textures[i].mipLevels, 1, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textures[i].image, textures[i].imageMemory);

            transitionImageLayout(textures[i].image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, textures[i].mipLevels);
            copyBufferToImage(stagingBuffer, textures[i].image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
            
            generateMipmaps(textures[i].image, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, textures[i].mipLevels);
            
            vkDestroyBuffer(device, stagingBuffer, nullptr);
            vkFreeMemory(device, stagingBufferMemory, nullptr);
        }
    }

    void createTextureImageViews() {
        for (int i = 0; i < TEXTURE_COUNT; i++)
            textures[i].imageView = createImageView(textures[i].image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, textures[i].mipLevels);
    }

    void createTextureSampler() {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = static_cast<float>(12);
        samplerInfo.mipLodBias = 0.0f;

        if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create texture sampler!");
        }
    }

    void createSkyboxSampler() {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.minLod = 0.0f; //Optional
        samplerInfo.maxLod = 0.0f; //static_cast<float>(mipLevels);
        samplerInfo.mipLodBias = 0.0f; //Optional

        if (vkCreateSampler(device, &samplerInfo, nullptr, &skyboxSampler) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create texture sampler!");
        }
    }

    void createCompositionSampler() {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.minLod = 0.0f; //Optional
        samplerInfo.maxLod = 0.0f; //static_cast<float>(mipLevels);
        samplerInfo.mipLodBias = 0.0f; //Optional

        if (vkCreateSampler(device, &samplerInfo, nullptr, &compositionSampler) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create texture sampler!");
        }
    }

    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView imageView;
        if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create texture image view!");
        }

        return imageView;
    }

    void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {
        //Check if image format supports linear blitting
        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, imageFormat, &formatProperties);

        if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
            throw std::runtime_error("Texture image format does not support linear blitting!");
        }

        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.subresourceRange.levelCount = 1;

        int32_t mipWidth = texWidth;
        int32_t mipHeight = texHeight;

        for (uint32_t i = 1; i < mipLevels; i++) {
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            vkCmdPipelineBarrier(commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                0, nullptr,
                0, nullptr,
                1, &barrier);

            VkImageBlit blit{};
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;

            vkCmdBlitImage(commandBuffer,
                image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit,
                VK_FILTER_LINEAR);

            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                0, nullptr,
                0, nullptr,
                1, &barrier);

            if (mipWidth > 1) mipWidth /= 2;
            if (mipHeight > 1) mipHeight /= 2;
        }

        barrier.subresourceRange.baseMipLevel = mipLevels - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        endSingleTimeCommands(commandBuffer);
    }

    void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, int arrayLayers, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage &image, VkDeviceMemory &imageMemory) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = mipLevels;
        imageInfo.arrayLayers = arrayLayers;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate image memory!");
        }

        vkBindImageMemory(device, image, imageMemory, 0);
    }

    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = mipLevels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        } else {
            throw std::invalid_argument("Unsupported layout transition!");
        }

        vkCmdPipelineBarrier(
            commandBuffer,
            sourceStage, destinationStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        endSingleTimeCommands(commandBuffer);
    }

    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {
            width,
            height,
            1
        };

        vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        endSingleTimeCommands(commandBuffer);
    }

    glm::mat3 getTBN(std::vector<glm::vec3> trianglePositions, std::vector<glm::vec2> uvs, glm::vec3 normal) {
        glm::vec3 V0 = trianglePositions[0];
        glm::vec3 V1 = trianglePositions[1];
        glm::vec3 V2 = trianglePositions[2];

        glm::vec2 UV0 = uvs[0];
        glm::vec2 UV1 = uvs[1];
        glm::vec2 UV2 = uvs[2];

        // Calculate edges
        glm::vec3 E1 = V1 - V0;
        glm::vec3 E2 = V2 - V0;

        // Calculate UV differences
        glm::vec2 dUV1 = UV1 - UV0;
        glm::vec2 dUV2 = UV2 - UV0;

        // Calculate the determinant (r)
        float r = 1.0f / (dUV1.x * dUV2.y - dUV1.y * dUV2.x);

        // Calculate Tangent and Bitangent
        glm::vec3 T = r * (dUV2.y * E1 - dUV1.y * E2);
        glm::vec3 B = r * (-dUV2.x * E1 + dUV1.x * E2);

        // Normalize T and B
        T = glm::normalize(T);
        B = glm::cross(normal, T);

        // Use the provided normal vector (you can also calculate it if needed)
        glm::vec3 N = glm::normalize(normal);

        // Create TBN matrix
        glm::mat3 TBN = glm::mat3(T, B, N);

        return TBN;
    }

    void buildWorld() {
        const siv::PerlinNoise::seed_type noiseSeed = 0;
	    const siv::PerlinNoise noise { noiseSeed };

        lights.clear();

        float chunkData[WORLD_X+1][WORLD_Y+1][WORLD_Z+1];

        const float frequency = 0.1f;

        for (int i = 0; i <= WORLD_X; i++) {
            int x = i;
            for (int j = 0; j <= WORLD_Y; j++) {
                int y = j;
                for (int k = 0; k <= WORLD_Z; k++) {
                    int z = k;

                    glm::vec3 pos = glm::vec3(x, y, z);
                    float n = pos.z-(noise.noise3D_01(pos.x*frequency*10.0f, pos.y*frequency*10.0f, pos.z*frequency*10.0f)*0.1f+noise.noise2D_01(pos.x*frequency, pos.y*frequency)*0.9f) * WORLD_Z;

                    chunkData[i][j][k] = n;

                    // float brightness = (float) chunkData[i][j][k];
                    // lights.push_back(Light{
                    //     glm::vec3(x, y, z),
                    //     glm::vec3(brightness),
                    //     1.0f
                    // });
                }
            }
        }

        float textureFrequency = 1/5.0f;

        for (int i = 0; i < WORLD_X; i++) {
            int x = i;
            for (int j = 0; j < WORLD_Y; j++) {
                int y = j;
                for (int k = 0; k < WORLD_Z; k++) {
                    int z = k;

                    const glm::vec3 positions[8] = {
                        glm::vec3(x+0, y+0, z+0),
                        glm::vec3(x+1, y+0, z+0),
                        glm::vec3(x+1, y+0, z+1),
                        glm::vec3(x+0, y+0, z+1),
                        glm::vec3(x+0, y+1, z+0),
                        glm::vec3(x+1, y+1, z+0),
                        glm::vec3(x+1, y+1, z+1),
                        glm::vec3(x+0, y+1, z+1)
                    };

                    const float data[8] = {
                        chunkData[i+0][j+0][k+0],
                        chunkData[i+1][j+0][k+0],
                        chunkData[i+1][j+0][k+1],
                        chunkData[i+0][j+0][k+1],
                        chunkData[i+0][j+1][k+0],
                        chunkData[i+1][j+1][k+0],
                        chunkData[i+1][j+1][k+1],
                        chunkData[i+0][j+1][k+1]
                    };

                    GridCell cell;
                    memcpy(cell.vertex, positions, sizeof(glm::vec3)*8);
                    memcpy(cell.value, data, sizeof(float)*8);

                    float isoLevel = 0.5f;

                    std::vector<std::vector<glm::vec3>> vertexPositions = MarchingCubes::triangulateCell(cell, isoLevel);

                    for (int i = 0; i < vertexPositions.size(); i++) {
                        std::vector<glm::vec3> trianglePositions = vertexPositions[i];
                        std::vector<glm::vec2> uvs = {
                            glm::vec2(trianglePositions[0].x*textureFrequency, trianglePositions[0].y*textureFrequency),
                            glm::vec2(trianglePositions[1].x*textureFrequency, trianglePositions[1].y*textureFrequency),
                            glm::vec2(trianglePositions[2].x*textureFrequency, trianglePositions[2].y*textureFrequency)
                        };

                        glm::vec3 normal = glm::normalize(glm::cross(trianglePositions[1]-trianglePositions[0], trianglePositions[2]-trianglePositions[0]));

                        glm::mat3 TBN = getTBN(trianglePositions, uvs, normal);

                        for (int j = 0; j < 3; j++) {
                            vertices.push_back({trianglePositions[j], uvs[j], normal});
                        }

                        polygonInfos.push_back({TBN[0], TBN[1], TBN[2]});
                    }
                }
            }
        }

        //Create a square from 0 to 1 in both x and y //???
        std::vector<glm::vec3> squarePositions = {
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(1.0f, 0.0f, 0.0f),
            glm::vec3(1.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        };
        std::vector<glm::vec2> squareUVs = {
            glm::vec2(0.0f, 0.0f),
            glm::vec2(1.0f, 0.0f),
            glm::vec2(1.0f, 1.0f),
            glm::vec2(0.0f, 1.0f)
        };
        vertices.push_back({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}});
        vertices.push_back({{1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}});
        vertices.push_back({{1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}});
        polygonInfos.push_back({glm::vec3(1.0f), glm::vec3(1.0f), glm::vec3(1.0f)});

        vertices.push_back({{1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}});
        vertices.push_back({{0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}});
        vertices.push_back({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}});
        polygonInfos.push_back({glm::vec3(1.0f), glm::vec3(1.0f), glm::vec3(1.0f)});

        createVertexBuffer(vertexBuffer, vertexBufferMemory, vertices.size()*sizeof(Vertex), vertices.data());
        indices.push_back(0);
        createIndexBuffer(indexBuffer, indexBufferMemory, indices.size()*sizeof(uint32_t), indices.data());
        createPolygonBuffer();
    }

    void createVertexBuffer(VkBuffer &vertexBuffer, VkDeviceMemory &vertexBufferMemory, VkDeviceSize bufferSize, void *vertexData) {
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void *data;
        vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
            memcpy(data, vertexData, (size_t) bufferSize);
        vkUnmapMemory(device, stagingBufferMemory);

        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

        copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }

    void createIndexBuffer(VkBuffer &indexBuffer, VkDeviceMemory &indexBufferMemory, VkDeviceSize bufferSize, void *indexData) {   
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void *data;
        vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
            memcpy(data, indexData, (size_t) bufferSize);
        vkUnmapMemory(device, stagingBufferMemory);

        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

        copyBuffer(stagingBuffer, indexBuffer, bufferSize);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    } 

    void createPolygonBuffer() {
        PolygonInfoBufferObject polygonInfoBufferObject{};
        memcpy(polygonInfoBufferObject.polygonInfos, polygonInfos.data(), sizeof(PolygonInfo)*polygonInfos.size());

        VkDeviceSize bufferSize = sizeof(PolygonInfoBufferObject);

        createBuffer(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, polygonInfoBuffer, polygonInfoBufferMemory);

        vkMapMemory(device, polygonInfoBufferMemory, 0, bufferSize, 0, &polygonInfoBufferMapped);

        memcpy(polygonInfoBufferMapped, &polygonInfoBufferObject, (size_t) bufferSize);
    }

    void createLightVertexBuffer() {
        VkDeviceSize bufferSize = sizeof(LightVertex)*8*(MAX_LIGHTS+1);

        createBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, lightVertexBuffer, lightVertexBufferMemory);

        vkMapMemory(device, lightVertexBufferMemory, 0, bufferSize, 0, &lightVertexBufferMapped);
    }

    void createLightIndexBuffer() {
        VkDeviceSize bufferSize = sizeof(uint32_t)*36*(MAX_LIGHTS+1);

        createBuffer(bufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, lightIndexBuffer, lightIndexBufferMemory);

        vkMapMemory(device, lightIndexBufferMemory, 0, bufferSize, 0, &lightIndexBufferMapped);
    }

    void createSkyboxVertexBuffer() {
        std::vector<PositionVertex> vertices;

        //Positive x (front)
        vertices.push_back({{ 1.0f,  1.0f, -1.0f}});
        vertices.push_back({{ 1.0f, -1.0f,  1.0f}});
        vertices.push_back({{ 1.0f,  1.0f,  1.0f}});

        vertices.push_back({{ 1.0f,  1.0f, -1.0f}});
        vertices.push_back({{ 1.0f, -1.0f, -1.0f}});
        vertices.push_back({{ 1.0f, -1.0f,  1.0f}});

        //Negative x (back)
        vertices.push_back({{-1.0f, -1.0f, -1.0f}});
        vertices.push_back({{-1.0f,  1.0f,  1.0f}});
        vertices.push_back({{-1.0f, -1.0f,  1.0f}});

        vertices.push_back({{-1.0f, -1.0f, -1.0f}});
        vertices.push_back({{-1.0f,  1.0f, -1.0f}});
        vertices.push_back({{-1.0f,  1.0f,  1.0f}});

        //Positive y (left)
        vertices.push_back({{ 1.0f,  1.0f, -1.0f}});
        vertices.push_back({{ 1.0f,  1.0f,  1.0f}});
        vertices.push_back({{-1.0f,  1.0f,  1.0f}});

        vertices.push_back({{ 1.0f,  1.0f, -1.0f}});
        vertices.push_back({{-1.0f,  1.0f,  1.0f}});
        vertices.push_back({{-1.0f,  1.0f, -1.0f}});

        //Negative y (right)
        vertices.push_back({{-1.0f, -1.0f, -1.0f}});
        vertices.push_back({{-1.0f, -1.0f,  1.0f}});
        vertices.push_back({{ 1.0f, -1.0f,  1.0f}});

        vertices.push_back({{-1.0f, -1.0f, -1.0f}});
        vertices.push_back({{ 1.0f, -1.0f,  1.0f}});
        vertices.push_back({{ 1.0f, -1.0f, -1.0f}});

        //Positive z (top)
        vertices.push_back({{ 1.0f, -1.0f,  1.0f}});
        vertices.push_back({{-1.0f,  1.0f,  1.0f}});
        vertices.push_back({{ 1.0f,  1.0f,  1.0f}});

        vertices.push_back({{ 1.0f, -1.0f,  1.0f}});
        vertices.push_back({{-1.0f, -1.0f,  1.0f}});
        vertices.push_back({{-1.0f,  1.0f,  1.0f}});

        //Negative z (bottom)
        vertices.push_back({{-1.0f, -1.0f, -1.0f}});
        vertices.push_back({{ 1.0f,  1.0f, -1.0f}});
        vertices.push_back({{-1.0f,  1.0f, -1.0f}});

        vertices.push_back({{-1.0f, -1.0f, -1.0f}});
        vertices.push_back({{ 1.0f, -1.0f, -1.0f}});
        vertices.push_back({{ 1.0f,  1.0f, -1.0f}});

        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

        createVertexBuffer(skyboxVertexBuffer, skyboxVertexBufferMemory, bufferSize, vertices.data());
    }

    void createCompositionVertexBuffer() {
        std::vector<PositionVertex> vertices;

        vertices.push_back({{-3.0f, -1.0f, 0.0f}});
        vertices.push_back({{1.0f, 3.0f, 0.0f}});
        vertices.push_back({{1.0f, -1.0f, 0.0f}});
        vertices.push_back({{-3.0f, -1.0f, 0.0f}});
        vertices.push_back({{1.0f, -1.0f, 0.0f}});
        vertices.push_back({{1.0f, 3.0f, 0.0f}});

        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

        createVertexBuffer(compositionVertexBuffer, compositionVertexBufferMemory, bufferSize, vertices.data());
    }

    void createUniformBuffers() {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);

        uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);

            vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
        }
    }

    void createLightBuffer() {
        LightBufferObject lightBufferObject{};
        lightBufferObject.lightCount = static_cast<uint32_t>(lights.size());
        memcpy(lightBufferObject.lights, lights.data(), sizeof(Light)*lights.size());

        VkDeviceSize bufferSize = sizeof(LightBufferObject);

        createBuffer(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, lightBuffer, lightBufferMemory);

        vkMapMemory(device, lightBufferMemory, 0, bufferSize, 0, &lightBufferMapped);

        memcpy(lightBufferMapped, &lightBufferObject, (size_t) bufferSize);

        float radius = LIGHT_SIZE/2.0f;

        for (int i = 0; i < lights.size(); i++) {
            //Bottom of cube
            lightVertices.push_back({ lights[i].position+glm::vec3(-radius, -radius, -radius), lights[i].color, lights[i].strength });
            lightVertices.push_back({ lights[i].position+glm::vec3( radius, -radius, -radius), lights[i].color, lights[i].strength });
            lightVertices.push_back({ lights[i].position+glm::vec3(-radius,  radius, -radius), lights[i].color, lights[i].strength });
            lightVertices.push_back({ lights[i].position+glm::vec3( radius,  radius, -radius), lights[i].color, lights[i].strength });
            //Top of cube
            lightVertices.push_back({ lights[i].position+glm::vec3(-radius, -radius,  radius), lights[i].color, lights[i].strength });
            lightVertices.push_back({ lights[i].position+glm::vec3( radius, -radius,  radius), lights[i].color, lights[i].strength });
            lightVertices.push_back({ lights[i].position+glm::vec3(-radius,  radius,  radius), lights[i].color, lights[i].strength });
            lightVertices.push_back({ lights[i].position+glm::vec3( radius,  radius,  radius), lights[i].color, lights[i].strength });

            //Positive x (front)
            lightIndices.push_back(i*8+1);
            lightIndices.push_back(i*8+3);
            lightIndices.push_back(i*8+5);
            lightIndices.push_back(i*8+7);
            lightIndices.push_back(i*8+5);
            lightIndices.push_back(i*8+3);

            //Negative x (back)
            lightIndices.push_back(i*8+0);
            lightIndices.push_back(i*8+4);
            lightIndices.push_back(i*8+2);
            lightIndices.push_back(i*8+6);
            lightIndices.push_back(i*8+2);
            lightIndices.push_back(i*8+4);

            //Positive y (left)
            lightIndices.push_back(i*8+2);
            lightIndices.push_back(i*8+7);
            lightIndices.push_back(i*8+3);
            lightIndices.push_back(i*8+2);
            lightIndices.push_back(i*8+6);
            lightIndices.push_back(i*8+7);

            //Negative y (right)
            lightIndices.push_back(i*8+0);
            lightIndices.push_back(i*8+1);
            lightIndices.push_back(i*8+5);
            lightIndices.push_back(i*8+0);
            lightIndices.push_back(i*8+5);
            lightIndices.push_back(i*8+4);

            //Positive z (top)
            lightIndices.push_back(i*8+5);
            lightIndices.push_back(i*8+7);
            lightIndices.push_back(i*8+4);
            lightIndices.push_back(i*8+6);
            lightIndices.push_back(i*8+4);
            lightIndices.push_back(i*8+7);

            //Negative z (bottom)
            lightIndices.push_back(i*8+3);
            lightIndices.push_back(i*8+1);
            lightIndices.push_back(i*8+0);
            lightIndices.push_back(i*8+0);
            lightIndices.push_back(i*8+2);
            lightIndices.push_back(i*8+3);
        }

        memcpy(lightVertexBufferMapped, lightVertices.data(), sizeof(LightVertex)*lightVertices.size());
        memcpy(lightIndexBufferMapped, lightIndices.data(), sizeof(uint32_t)*lightIndices.size());
    }

    void createOffScreenDescriptorPool() {
        std::array<VkDescriptorPoolSize, 3> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT*TEXTURE_COUNT);
        poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[2].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &offScreenDescriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool!");
        }
    }

    void createComputeDescriptorPool() {
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &computeDescriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool!");
        }
    }

    void createOnScreenDescriptorPool() {
        std::array<VkDescriptorPoolSize, 3> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT*COLOR_ATTACHMENT_COUNT);
        poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[2].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &onScreenDescriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool!");
        }
    }

    void createOffScreenDescriptorSets() {
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, offScreenDescriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = offScreenDescriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();

        offScreenDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(device, &allocInfo, offScreenDescriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor sets!");
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo uniformBufferInfo{};
            uniformBufferInfo.buffer = uniformBuffers[i];
            uniformBufferInfo.offset = 0;
            uniformBufferInfo.range = sizeof(UniformBufferObject);

            VkDescriptorImageInfo textureImageInfos[TEXTURE_COUNT];
            for (int j = 0; j < TEXTURE_COUNT; j++) {
                textureImageInfos[j].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                textureImageInfos[j].imageView = textures[j].imageView;
                textureImageInfos[j].sampler = j < 6 ? skyboxSampler : textureSampler;
            }

            VkDescriptorBufferInfo polygonInfoBufferInfo{};
            polygonInfoBufferInfo.buffer = polygonInfoBuffer;
            polygonInfoBufferInfo.offset = 0;
            polygonInfoBufferInfo.range = sizeof(PolygonInfoBufferObject);

            std::array<VkWriteDescriptorSet, 3> descriptorWrites{};

            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = offScreenDescriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &uniformBufferInfo;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = offScreenDescriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].descriptorCount = TEXTURE_COUNT;
            descriptorWrites[1].pImageInfo = textureImageInfos;

            descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[2].dstSet = offScreenDescriptorSets[i];
            descriptorWrites[2].dstBinding = 2;
            descriptorWrites[2].dstArrayElement = 0;
            descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[2].descriptorCount = 1;
            descriptorWrites[2].pBufferInfo = &polygonInfoBufferInfo;

            vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

    void createComputeDescriptorSets() {
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, computeDescriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = computeDescriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();

        computeDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(device, &allocInfo, computeDescriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor sets!");
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorImageInfo firstBloomImageInfo{};
            firstBloomImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            firstBloomImageInfo.imageView = bloomImageViews[0];
            firstBloomImageInfo.sampler = compositionSampler;
            
            VkDescriptorImageInfo secondBloomImageInfo{};
            secondBloomImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            secondBloomImageInfo.imageView = bloomImageViews[1];
            secondBloomImageInfo.sampler = compositionSampler;

            std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = computeDescriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pImageInfo = &firstBloomImageInfo;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = computeDescriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &secondBloomImageInfo;

            vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

    void createOnScreenDescriptorSets() {
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, onScreenDescriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = onScreenDescriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();

        onScreenDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(device, &allocInfo, onScreenDescriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor sets!");
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo uniformBufferInfo{};
            uniformBufferInfo.buffer = uniformBuffers[i];
            uniformBufferInfo.offset = 0;
            uniformBufferInfo.range = sizeof(UniformBufferObject);

            VkDescriptorImageInfo compositionImageInfo[COLOR_ATTACHMENT_COUNT];
            compositionImageInfo[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            compositionImageInfo[0].imageView = positionAttachment.view;
            compositionImageInfo[0].sampler = compositionSampler;
            compositionImageInfo[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            compositionImageInfo[1].imageView = colorAttachment.view;
            compositionImageInfo[1].sampler = compositionSampler;
            compositionImageInfo[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            compositionImageInfo[2].imageView = normalAttachment.view;
            compositionImageInfo[2].sampler = compositionSampler;
            compositionImageInfo[3].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            compositionImageInfo[3].imageView = bloomImageViews[0];
            compositionImageInfo[3].sampler = compositionSampler;

            VkDescriptorBufferInfo lightBufferInfo{};
            lightBufferInfo.buffer = lightBuffer;
            lightBufferInfo.offset = 0;
            lightBufferInfo.range = sizeof(LightBufferObject);

            std::array<VkWriteDescriptorSet, 3> descriptorWrites{};

            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = onScreenDescriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &uniformBufferInfo;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = onScreenDescriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].descriptorCount = COLOR_ATTACHMENT_COUNT;
            descriptorWrites[1].pImageInfo = compositionImageInfo;

            descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[2].dstSet = onScreenDescriptorSets[i];
            descriptorWrites[2].dstBinding = 2;
            descriptorWrites[2].dstArrayElement = 0;
            descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[2].descriptorCount = 1;
            descriptorWrites[2].pBufferInfo = &lightBufferInfo;

            vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

    void recreateComputeDescriptorSets() {
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorImageInfo firstBloomImageInfo{};
            firstBloomImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            firstBloomImageInfo.imageView = bloomImageViews[0];
            firstBloomImageInfo.sampler = compositionSampler;
            
            VkDescriptorImageInfo secondBloomImageInfo{};
            secondBloomImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            secondBloomImageInfo.imageView = bloomImageViews[1];
            secondBloomImageInfo.sampler = compositionSampler;

            std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = computeDescriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pImageInfo = &firstBloomImageInfo;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = computeDescriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &secondBloomImageInfo;

            vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

    void recreateOnScreenDescriptorSets() {
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorImageInfo compositionImageInfo[COLOR_ATTACHMENT_COUNT];
            compositionImageInfo[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            compositionImageInfo[0].imageView = positionAttachment.view;
            compositionImageInfo[0].sampler = compositionSampler;
            compositionImageInfo[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            compositionImageInfo[1].imageView = colorAttachment.view;
            compositionImageInfo[1].sampler = compositionSampler;
            compositionImageInfo[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            compositionImageInfo[2].imageView = normalAttachment.view;
            compositionImageInfo[2].sampler = compositionSampler;
            compositionImageInfo[3].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            compositionImageInfo[3].imageView = bloomImageViews[0];
            compositionImageInfo[3].sampler = compositionSampler;

            std::array<VkWriteDescriptorSet, 1> descriptorWrites{};

            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = onScreenDescriptorSets[i];
            descriptorWrites[0].dstBinding = 1;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[0].descriptorCount = COLOR_ATTACHMENT_COUNT;
            descriptorWrites[0].pImageInfo = compositionImageInfo;

            vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &bufferMemory) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate buffer memory!");
        }

        vkBindBufferMemory(device, buffer, bufferMemory, 0);
    }

    VkCommandBuffer beginSingleTimeCommands() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);

        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }
    
    VkSampleCountFlagBits getMaxUsableSampleCount() {
        VkPhysicalDeviceProperties physicalDeviceProperties;
        vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

        VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
        if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
        if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
        if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
        if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
        if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
        if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

        return VK_SAMPLE_COUNT_1_BIT;
    }

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        endSingleTimeCommands(commandBuffer);
    }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("Failed to find suitable memory type!");
    }

    void createOffScreenCommandBuffers() {
        offScreenCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t) offScreenCommandBuffers.size();;

        if (vkAllocateCommandBuffers(device, &allocInfo, offScreenCommandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate off screen command buffer!");
        }
    }//!!!Unused

    void createComputeCommandBuffers() {
        computeCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t) computeCommandBuffers.size();

        if (vkAllocateCommandBuffers(device, &allocInfo, computeCommandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate compute command buffers!");
        }
    }

    void createCommandBuffers() {
        commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

        if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffers!");
        }
    }

    void recordOffScreenCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = offScreenRenderPass;
        renderPassInfo.framebuffer = gBuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChainExtent;

        VkClearValue clearValues[COLOR_ATTACHMENT_COUNT+1];
        for (int i = 0; i < COLOR_ATTACHMENT_COUNT; i++)
            clearValues[i].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
        clearValues[COLOR_ATTACHMENT_COUNT].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(COLOR_ATTACHMENT_COUNT+1);
        renderPassInfo.pClearValues = clearValues;
        
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = (float) swapChainExtent.width;
            viewport.height = (float) swapChainExtent.height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = swapChainExtent;
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
            
            {
            VkBuffer vertexBuffers[] = {vertexBuffer};
            VkDeviceSize offsets[] = {0};

            struct {
                uint32_t isWater;
                uint32_t isReflect;
            } waterPushConstant;
            waterPushConstant.isWater = false;
            waterPushConstant.isReflect = false;

            vkCmdPushConstants(commandBuffer, mainPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(waterPushConstant), &waterPushConstant);
            
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mainPipelineLayout, 0, 1, &offScreenDescriptorSets[currentFrame], 0, nullptr);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mainPipeline);
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

            vkCmdDraw(commandBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
            }

            {
            VkBuffer vertexBuffers[] = {lightVertexBuffer};
            VkDeviceSize offsets[] = {0};

            struct {
                uint32_t isWater;
                uint32_t isReflect;
            } waterPushConstant;
            waterPushConstant.isWater = false;
            waterPushConstant.isReflect = false;

            vkCmdPushConstants(commandBuffer, lightPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(waterPushConstant), &waterPushConstant);

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightPipelineLayout, 0, 1, &offScreenDescriptorSets[currentFrame], 0, nullptr);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightPipeline);
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, lightIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(lightIndices.size()+isDay*36), 1, 0, 0, 0);
            }

            {
            VkBuffer vertexBuffers[] = {skyboxVertexBuffer};
            VkDeviceSize offsets[] = {0};

            struct {
                uint32_t isWater;
                uint32_t isReflect;
                uint32_t isDay;
            } waterAndDayPushConstant;
            waterAndDayPushConstant.isWater = false;
            waterAndDayPushConstant.isReflect = false;
            waterAndDayPushConstant.isDay = (uint32_t) isDay;

            vkCmdPushConstants(commandBuffer, skyboxPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(waterAndDayPushConstant), &waterAndDayPushConstant);

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipelineLayout, 0, 1, &offScreenDescriptorSets[currentFrame], 0, nullptr);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline);
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

            vkCmdDraw(commandBuffer, static_cast<uint32_t>(36), 1, 0, 0);         
            }

        vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to record command buffer!");
        }
    }

    void resetBloom(VkCommandBuffer commandBuffer) {
        VkClearColorValue clearValueColor = {{0.0f, 0.0f, 0.0f, 0.0f}};
        VkImageSubresourceRange clearRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkCmdClearColorImage(commandBuffer, bloomImages[1], VK_IMAGE_LAYOUT_GENERAL, &clearValueColor, 1, &clearRange);
    }

    void blitToBloom(VkCommandBuffer commandBuffer) {
        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, bloomAttachment.format, &formatProperties);

        if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
            throw std::runtime_error("Texture image format does not support linear blitting!");
        }

        VkClearColorValue clearValueColor = {{0.0f, 0.0f, 0.0f, 0.0f}};
        VkImageSubresourceRange clearRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkCmdClearColorImage(commandBuffer, bloomImages[1], VK_IMAGE_LAYOUT_GENERAL, &clearValueColor, 1, &clearRange);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = bloomAttachment.image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        barrier.image = bloomImages[0];
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        
        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {(int32_t) swapChainExtent.width, (int32_t) swapChainExtent.height, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = 0;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {(int32_t) bloomImageExtent.width, (int32_t) bloomImageExtent.height, 1};
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = 0;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(commandBuffer,
            bloomAttachment.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            bloomImages[0], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit,
            VK_FILTER_LINEAR);

        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        barrier.image = bloomAttachment.image;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        barrier.image = bloomImages[0];
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);
    }

    void prepareBloomImages() {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        resetBloom(commandBuffer);
        blitToBloom(commandBuffer);        

        endSingleTimeCommands(commandBuffer);
    }

    void recordComputeCommandBuffer(VkCommandBuffer commandBuffer) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin recording compute command buffer!");
        }
        
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, bloomPipeline);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, bloomPipelineLayout, 0, 1, &computeDescriptorSets[currentFrame], 0, nullptr);

        {
        struct {
            uint32_t width;
            uint32_t height;
            uint32_t horizontal;
        } pushConstant;
        pushConstant.width = swapChainExtent.width;
        pushConstant.height = swapChainExtent.height;
        pushConstant.horizontal = (uint32_t) true;

        vkCmdPushConstants(commandBuffer, bloomPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstant), &pushConstant);

        vkCmdDispatch(commandBuffer, swapChainExtent.width, swapChainExtent.height, 1);
        }

        {
        struct {
            uint32_t width;
            uint32_t height;
            uint32_t horizontal;
        } pushConstant;
        pushConstant.width = swapChainExtent.width;
        pushConstant.height = swapChainExtent.height;
        pushConstant.horizontal = (uint32_t) false;

        vkCmdPushConstants(commandBuffer, bloomPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstant), &pushConstant);

        vkCmdDispatch(commandBuffer, swapChainExtent.width, swapChainExtent.height, 1);
        }

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to record compute command buffer!");
        }
    }

    void recordOnScreenCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = onScreenRenderPass;
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = {swapChainExtent.width, swapChainExtent.height};

        std::array<VkClearValue, 1> clearValues{};
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};

        renderPassInfo.clearValueCount = COLOR_ATTACHMENT_COUNT;//static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();
        
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = (float) swapChainExtent.width;
            viewport.height = (float) swapChainExtent.height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = swapChainExtent;
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            {
            VkBuffer vertexBuffers[] = {compositionVertexBuffer};
            VkDeviceSize offsets[] = {0};

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, compositionPipelineLayout, 0, 1, &onScreenDescriptorSets[currentFrame], 0, nullptr);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, compositionPipeline);
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &compositionVertexBuffer, offsets);

            struct {
                uint32_t isDay;
            } pushConstant;
            pushConstant.isDay = (uint32_t) isDay;

            vkCmdPushConstants(commandBuffer, compositionPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConstant), &pushConstant);

            vkCmdDraw(commandBuffer, static_cast<uint32_t>(6), 1, 0, 0);   
            }
            
        vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to record command buffer!");
        }
    }

    void createSyncObjects() {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        offScreenRenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        computeFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        onScreenRenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        computeInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        deferredInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device, &semaphoreInfo, nullptr, &offScreenRenderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device, &semaphoreInfo, nullptr, &onScreenRenderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS ||
                vkCreateFence(device, &fenceInfo, nullptr, &deferredInFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create synchronization objects for a frame!");
            }

            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &computeFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(device, &fenceInfo, nullptr, &computeInFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create compute synchronization objects for a frame!");
            }

            vkResetFences(device, 1, &computeInFlightFences[i]);
            vkResetFences(device, 1, &deferredInFlightFences[i]);
        }
    }

    void updateUniformBuffer(uint32_t currentImage) {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        if (viewAngles.y >= 180)
            viewAngles.y = 180-viewAngles.y;
        if (viewAngles.y >= 80.0f)
            viewAngles.y = 80.0f;
        else if (viewAngles.y <= -80.0f)
            viewAngles.y = -80.0f;

        viewAngles.x = fmod(viewAngles.x, 360.0f);
        viewAngles.y = fmod(viewAngles.y, 360.0f);
        viewDirection = glm::normalize(glm::vec3(
            std::cos(glm::radians(-viewAngles.x))*std::cos(glm::radians(viewAngles.y)), 
            std::sin(glm::radians(-viewAngles.x))*std::cos(glm::radians(viewAngles.y)), 
            std::sin(glm::radians(viewAngles.y))
        ));

        UniformBufferObject ubo{};

        ubo.view = glm::lookAt(cameraPosition, cameraPosition+viewDirection, glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.projection = glm::perspective(glm::radians(FOV), swapChainExtent.width / (float) swapChainExtent.height, nearPlane, farPlane);
        ubo.projection[1][1] *= -1;

        glm::vec3 reflectCameraPosition = glm::vec3(cameraPosition.x , cameraPosition.y, cameraPosition.z-2.0f*(cameraPosition.z-SEA_LEVEL));
        glm::vec3 reflectViewDirection = glm::vec3(viewDirection.x, viewDirection.y, -viewDirection.z);

        ubo.reflectView = glm::lookAt(reflectCameraPosition, reflectCameraPosition+reflectViewDirection, glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.reflectProjection = ubo.projection;

        ubo.refractView = ubo.reflectView;
        ubo.refractProjection = ubo.projection;

        //TEMP
        //isDay = glfwGetKey(window, GLFW_KEY_T) != GLFW_PRESS;
        if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS) {
            ubo.view = ubo.refractView;
            ubo.projection = ubo.reflectProjection;
        }

        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
            ubo.view = ubo.reflectView;
            ubo.projection = ubo.reflectProjection;
        }

        ubo.cameraPosition = cameraPosition;
        ubo.viewDirection = viewDirection;

        ubo.nearPlane = nearPlane;
        ubo.farPlane = farPlane;

        ubo.time = time;

        ubo.gamma = 2.2f;
        ubo.exposure = exposure;

        memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    }

    void updateLightBuffer() {
        LightBufferObject lightBufferObject{};
        lightBufferObject.lightCount = static_cast<uint32_t>(lights.size());
        for (int i = 0; i < lights.size(); i++) {
            lightBufferObject.lights[i] = lights[i];
        }

        VkDeviceSize bufferSize = sizeof(LightBufferObject);

        memcpy(lightBufferMapped, &lightBufferObject, (size_t) bufferSize);
        
        lightVertices.clear();
        lightIndices.clear();

        float radius = LIGHT_SIZE/2.0f;

        for (int i = 0; i < lights.size(); i++) {
            //Bottom of cube
            lightVertices.push_back({ lights[i].position+glm::vec3(-radius, -radius, -radius), lights[i].color, lights[i].strength });
            lightVertices.push_back({ lights[i].position+glm::vec3( radius, -radius, -radius), lights[i].color, lights[i].strength });
            lightVertices.push_back({ lights[i].position+glm::vec3(-radius,  radius, -radius), lights[i].color, lights[i].strength });
            lightVertices.push_back({ lights[i].position+glm::vec3( radius,  radius, -radius), lights[i].color, lights[i].strength });
            //Top of cube
            lightVertices.push_back({ lights[i].position+glm::vec3(-radius, -radius,  radius), lights[i].color, lights[i].strength });
            lightVertices.push_back({ lights[i].position+glm::vec3( radius, -radius,  radius), lights[i].color, lights[i].strength });
            lightVertices.push_back({ lights[i].position+glm::vec3(-radius,  radius,  radius), lights[i].color, lights[i].strength });
            lightVertices.push_back({ lights[i].position+glm::vec3( radius,  radius,  radius), lights[i].color, lights[i].strength });

            //Positive x (front)
            lightIndices.push_back(i*8+1);
            lightIndices.push_back(i*8+3);
            lightIndices.push_back(i*8+5);
            lightIndices.push_back(i*8+7);
            lightIndices.push_back(i*8+5);
            lightIndices.push_back(i*8+3);

            //Negative x (back)
            lightIndices.push_back(i*8+0);
            lightIndices.push_back(i*8+4);
            lightIndices.push_back(i*8+2);
            lightIndices.push_back(i*8+6);
            lightIndices.push_back(i*8+2);
            lightIndices.push_back(i*8+4);

            //Positive y (left)
            lightIndices.push_back(i*8+2);
            lightIndices.push_back(i*8+7);
            lightIndices.push_back(i*8+3);
            lightIndices.push_back(i*8+2);
            lightIndices.push_back(i*8+6);
            lightIndices.push_back(i*8+7);

            //Negative y (right)
            lightIndices.push_back(i*8+0);
            lightIndices.push_back(i*8+1);
            lightIndices.push_back(i*8+5);
            lightIndices.push_back(i*8+0);
            lightIndices.push_back(i*8+5);
            lightIndices.push_back(i*8+4);

            //Positive z (top)
            lightIndices.push_back(i*8+5);
            lightIndices.push_back(i*8+7);
            lightIndices.push_back(i*8+4);
            lightIndices.push_back(i*8+6);
            lightIndices.push_back(i*8+4);
            lightIndices.push_back(i*8+7);

            //Negative z (bottom)
            lightIndices.push_back(i*8+3);
            lightIndices.push_back(i*8+1);
            lightIndices.push_back(i*8+0);
            lightIndices.push_back(i*8+0);
            lightIndices.push_back(i*8+2);
            lightIndices.push_back(i*8+3);
        }

        memcpy(lightVertexBufferMapped, lightVertices.data(), sizeof(Vertex)*lightVertices.size());
        memcpy(lightIndexBufferMapped, lightIndices.data(), sizeof(uint32_t)*lightIndices.size());

        isLightsChanged = false;
    }

    void updateSun() {
        float radius = 100.0f;

        Light sun;
        sun.position = glm::vec3(40, 40, 20)+500.0f*glm::vec3(sin(time), cos(time), 1.0f);
        sun.color = glm::vec3(0.8f, 1.0f, 0.8f);
        sun.strength = 1.0f;

        uint32_t originalVertexCount = static_cast<uint32_t>(lightVertices.size());

        std::vector<LightVertex> sunVertices;
        std::vector<uint32_t> sunIndices;

        //Bottom of cube
        sunVertices.push_back({ sun.position+glm::vec3(-radius, -radius, -radius), sun.color, sun.strength });
        sunVertices.push_back({ sun.position+glm::vec3( radius, -radius, -radius), sun.color, sun.strength });
        sunVertices.push_back({ sun.position+glm::vec3(-radius,  radius, -radius), sun.color, sun.strength });
        sunVertices.push_back({ sun.position+glm::vec3( radius,  radius, -radius), sun.color, sun.strength });
        //Top of cube
        sunVertices.push_back({ sun.position+glm::vec3(-radius, -radius,  radius), sun.color, sun.strength });
        sunVertices.push_back({ sun.position+glm::vec3( radius, -radius,  radius), sun.color, sun.strength });
        sunVertices.push_back({ sun.position+glm::vec3(-radius,  radius,  radius), sun.color, sun.strength });
        sunVertices.push_back({ sun.position+glm::vec3( radius,  radius,  radius), sun.color, sun.strength });

        //Positive x (front)
        sunIndices.push_back(1);
        sunIndices.push_back(3);
        sunIndices.push_back(5);
        sunIndices.push_back(7);
        sunIndices.push_back(5);
        sunIndices.push_back(3);

        //Negative x (back)
        sunIndices.push_back(0);
        sunIndices.push_back(4);
        sunIndices.push_back(2);
        sunIndices.push_back(6);
        sunIndices.push_back(2);
        sunIndices.push_back(4);

        //Positive y (left)
        sunIndices.push_back(2);
        sunIndices.push_back(7);
        sunIndices.push_back(3);
        sunIndices.push_back(2);
        sunIndices.push_back(6);
        sunIndices.push_back(7);

        //Negative y (right)
        sunIndices.push_back(0);
        sunIndices.push_back(1);
        sunIndices.push_back(5);
        sunIndices.push_back(0);
        sunIndices.push_back(5);
        sunIndices.push_back(4);

        //Positive z (top)
        sunIndices.push_back(5);
        sunIndices.push_back(7);
        sunIndices.push_back(4);
        sunIndices.push_back(6);
        sunIndices.push_back(4);
        sunIndices.push_back(7);

        //Negative z (bottom)
        sunIndices.push_back(3);
        sunIndices.push_back(1);
        sunIndices.push_back(0);
        sunIndices.push_back(0);
        sunIndices.push_back(2);
        sunIndices.push_back(3);

        memcpy((LightVertex*) (lightVertexBufferMapped)+originalVertexCount*sizeof(LightVertex), sunVertices.data(), sizeof(LightVertex)*sunVertices.size());
        memcpy((uint32_t*) lightIndexBufferMapped+originalVertexCount*sizeof(uint32_t)*9/2, sunIndices.data(), sizeof(uint32_t)*sunIndices.size());
    }

    void update() {
        //std::cout << viewAngles.x << " " << viewAngles.y << std::endl;
    }

    void drawFrame() {
        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore signalSemaphores[] = {offScreenRenderFinishedSemaphores[currentFrame], computeFinishedSemaphores[currentFrame], onScreenRenderFinishedSemaphores[currentFrame]};

        uint32_t imageIndex;
        
        //Get image to be presented
        VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
        
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("Failed to acquire swap chain image!");
        }

        //Off screen render
        {
        updateUniformBuffer(currentFrame);
        if (isLightsChanged)
            updateLightBuffer();
        if (isDay)
            updateSun();

        vkResetFences(device, 1, &inFlightFences[currentFrame]);

        vkResetCommandBuffer(commandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
        recordOffScreenCommandBuffer(commandBuffers[currentFrame], imageIndex);

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &signalSemaphores[1];

        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, deferredInFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit draw command buffer for off screen render!");
        }
        }

        //Compute 
        {
        // vkWaitForFences(device, 1, &computeInFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        // prepareBloomImages();
        // recordComputeCommandBuffer(computeCommandBuffers[currentFrame]);

        // vkResetFences(device, 1, &computeInFlightFences[currentFrame]);

        // VkSemaphore waitSemaphores[] = {signalSemaphores[0]};
        // VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        // submitInfo.waitSemaphoreCount = 1;
        // submitInfo.pWaitSemaphores = waitSemaphores;
        // submitInfo.pWaitDstStageMask = waitStages;

        // submitInfo.commandBufferCount = 1;
        // submitInfo.pCommandBuffers = &computeCommandBuffers[currentFrame];

        // submitInfo.signalSemaphoreCount = 1;
        // submitInfo.pSignalSemaphores = &signalSemaphores[1];

        // if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, deferredInFlightFences[currentFrame]) != VK_SUCCESS) {
        //     throw std::runtime_error("Failed to submit compute command buffer for off screen render!");
        // }
        }

        //On screen render
        {
        vkWaitForFences(device, 1, &deferredInFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        recordOnScreenCommandBuffer(commandBuffers[currentFrame], imageIndex);

        vkResetFences(device, 1, &deferredInFlightFences[currentFrame]);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {signalSemaphores[1]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &signalSemaphores[2];

        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit draw command buffer for on screen render!");
        }
        }

        //Present
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &signalSemaphores[2];

        VkSwapchainKHR swapChains[] = {swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;

        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(presentQueue, &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = false;
            recreateSwapChain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to present swap chain image!");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    VkShaderModule createShaderModule(const std::vector<char> &code) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module!");
        }

        return shaderModule;
    }

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        for (const auto &availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) {
        for (const auto &availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
        SwapChainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    bool isDeviceSuitable(VkPhysicalDevice device) {
        QueueFamilyIndices indices = findQueueFamilies(device);

        bool extensionsSupported = checkDeviceExtensionSupport(device);

        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

        return indices.isComplete() && extensionsSupported && swapChainAdequate  && supportedFeatures.samplerAnisotropy && supportedFeatures.shaderClipDistance;
    }

    bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto &extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto &queueFamily : queueFamilies) {
            if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) && (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
                indices.graphicsAndComputeFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

            if (presentSupport) {
                indices.presentFamily = i;
            }

            if (indices.isComplete()) {
                break;
            }

            i++;
        }

        return indices;
    }

    std::vector<const char*> getRequiredExtensions() {
        uint32_t glfwExtensionCount = 0;
        const char **glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    bool checkValidationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char *layerName : validationLayers) {
            bool layerFound = false;

            for (const auto &layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) {
                return false;
            }
        }

        return true;
    }

    static std::vector<char> readFile(const std::string &filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file!");
        }

        size_t fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        return buffer;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData) {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }
};

int main() {
    VulkanTerrainApplication app;

    try {
        app.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}