#ifndef VK_UTILS
#define VK_UTILS

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include <cmath>
#include <functional>

namespace VkUtils {
    enum Orientation {POSITIVE_X, NEGATIVE_X, POSITIVE_Y, NEGATIVE_Y, POSITIVE_Z, NEGATIVE_Z, ORIENTATION_MAX_ENUM};

    struct Vertex {
        glm::vec3 position;
        glm::vec2 textureCoordinates;
        glm::vec3 normal;

        static inline VkVertexInputBindingDescription getBindingDescription() {
            VkVertexInputBindingDescription bindingDescription{};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(Vertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            return bindingDescription;
        }

        static inline std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
            std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

            attributeDescriptions[0].binding = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[0].offset = offsetof(Vertex, position);

            attributeDescriptions[1].binding = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
            attributeDescriptions[1].offset = offsetof(Vertex, textureCoordinates);

            attributeDescriptions[2].binding = 0;
            attributeDescriptions[2].location = 2;
            attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[2].offset = offsetof(Vertex, normal);

            return attributeDescriptions;
        }

        inline bool operator==(const Vertex &other) const {
            return position == other.position && textureCoordinates == other.textureCoordinates && normal == other.normal;
        }
    };

    struct LightVertex {
        glm::vec3 position;
        glm::vec3 color;
        float strength;

        static inline VkVertexInputBindingDescription getBindingDescription() {
            VkVertexInputBindingDescription bindingDescription{};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(LightVertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            return bindingDescription;
        }

        static inline std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
            std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

            attributeDescriptions[0].binding = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[0].offset = offsetof(LightVertex, position);

            attributeDescriptions[1].binding = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[1].offset = offsetof(LightVertex, color);

            attributeDescriptions[2].binding = 0;
            attributeDescriptions[2].location = 2;
            attributeDescriptions[2].format = VK_FORMAT_R32_SFLOAT;
            attributeDescriptions[2].offset = offsetof(LightVertex, strength);

            return attributeDescriptions;
        }

        inline bool operator==(const LightVertex &other) const {
            return position == other.position && color == other.color && strength == other.strength;
        }
    };

    struct PositionVertex {
        glm::vec3 position;

        static inline VkVertexInputBindingDescription getBindingDescription() {
            VkVertexInputBindingDescription bindingDescription{};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(PositionVertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            return bindingDescription;
        }

        static inline std::array<VkVertexInputAttributeDescription, 1> getAttributeDescriptions() {
            std::array<VkVertexInputAttributeDescription, 1> attributeDescriptions{};

            attributeDescriptions[0].binding = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[0].offset = offsetof(PositionVertex, position);

            return attributeDescriptions;
        }

        inline bool operator==(const PositionVertex &other) const {
            return position == other.position;
        }
    };

    struct GPUTexture {
        VkSampler albedoSampler;
        VkSampler normalSampler;
    };

    struct Texture {
        VkImage image;
        VkDeviceMemory imageMemory;
        VkImageView imageView;
        uint32_t index;
        uint32_t mipLevels;

        inline void cleanup(VkDevice device) {
            vkDestroyImageView(device, imageView, nullptr);
            vkDestroyImage(device, image, nullptr);
            vkFreeMemory(device, imageMemory, nullptr);
        }
    };

    struct FrameBufferAttachment {
        VkImage image;
        VkDeviceMemory memory;
        VkImageView view;
        VkFormat format;

        void cleanup(VkDevice device) {
            vkDestroyImageView(device, view, nullptr);
            vkDestroyImage(device, image, nullptr);
            vkFreeMemory(device, memory, nullptr);
        }
    };

    struct OffScreenRenderPass {
        VkRenderPass renderPass;
        FrameBufferAttachment positionAttachment;
        FrameBufferAttachment colorAttachment;
        FrameBufferAttachment normalAttachment;
        FrameBufferAttachment bloomAttachment;
        FrameBufferAttachment depthAttachment;

        VkPipeline mainPipeline;
        VkPipeline lightPipeline;
        VkPipeline skyboxPipeline;

        void cleanup(VkDevice device) {
            positionAttachment.cleanup(device);
            colorAttachment.cleanup(device);
            normalAttachment.cleanup(device);
            bloomAttachment.cleanup(device);
            depthAttachment.cleanup(device);

            vkDestroyPipeline(device, mainPipeline, nullptr);
            vkDestroyPipeline(device, lightPipeline, nullptr);
            vkDestroyPipeline(device, skyboxPipeline, nullptr);

            vkDestroyRenderPass(device, renderPass, nullptr);
        }
    };
}

#endif