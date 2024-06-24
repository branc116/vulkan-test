#include "vulkan/vulkan.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

char* read_entire_file(const char* path, long* len) {
  FILE* file = fopen(path, "rb");
  if (NULL == file) {
    fprintf(stderr, "Failed to open file %s: %s\n", path, strerror(errno));
    return NULL;
  }
  fseek(file, 0, SEEK_END);
  *len = ftell(file);
  char * buff = malloc(*len + 1);
  fseek(file, 0, SEEK_SET);
  fread(buff, 1, *len, file);
  return buff;
}

VkBool32 vlog(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
  printf("[%d, %d] %s\n", messageSeverity, messageTypes, pCallbackData->pMessage);
  return VK_FALSE;
}

static inline VkResult load_shader(VkDevice device, const char* path, VkShaderModule* module) {
  long len = 0;
  char* str = read_entire_file(path, &len);
  if (str == NULL) return VK_ERROR_DEVICE_LOST;
  VkShaderModuleCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .pCode = (uint32_t*)str,
    .codeSize = len
  };
  VkResult res = vkCreateShaderModule(device, &info, NULL, module);
  free(str);
  return res;
}
const char * val_layers = "VK_LAYER_KHRONOS_validation";

int main(void) {
  VkResult result = 0;
  int w = 800, h = 640;
  glfwInit();

  VkInstance instance = {0};
  {
    uint32_t count;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);
    const char** real_exs = malloc((count + 1) * sizeof(real_exs[0]));
    memcpy(real_exs, extensions, sizeof(extensions[0]) * count);
    real_exs[count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    VkInstanceCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .enabledExtensionCount = count,
      .ppEnabledExtensionNames = real_exs,
      .pApplicationInfo = &(VkApplicationInfo) {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Dick Butt",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Dicky",
        .engineVersion = VK_MAKE_VERSION(0, 0, 0),
        .apiVersion = VK_API_VERSION_1_0
      },
      .enabledLayerCount = 1,
      .ppEnabledLayerNames = &val_layers,
      .pNext = &(VkDebugUtilsMessengerCreateInfoEXT) {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
          VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT,
        .pfnUserCallback = vlog,
        .pNext = 0
      }
    };
    if ((result = vkCreateInstance(&info, NULL, &instance))) goto error;
  }
  printf("Instance created\n");
  GLFWwindow* window = NULL;
  VkSurfaceKHR surface = {0};
  {
    glfwInitVulkanLoader(vkGetInstanceProcAddr);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(w, h, "Window Title", NULL, NULL);
    glfwSetWindowAttrib(window, GLFW_FLOATING, 1);
    if ((result = glfwCreateWindowSurface(instance, window, NULL, &surface))) goto error;
  }
  VkPhysicalDevice phy_device = NULL;
  {
    if ((result = vkEnumeratePhysicalDevices(instance, &(uint32_t){1}, &phy_device))) goto error;
  }
  printf("Phy  created\n");
  VkDevice device = {0};
  VkQueue qraphics, qresent = {0};
  {
    const char * const exsts =  VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    VkDeviceCreateInfo dev_create_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .enabledExtensionCount = 1,
      .ppEnabledExtensionNames = &exsts,
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &(VkDeviceQueueCreateInfo) { 
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .queueFamilyIndex = 0, .queueCount = 1, .pQueuePriorities = &(float){1.0f}
      },
      .pEnabledFeatures = &(VkPhysicalDeviceFeatures){0},
      .enabledLayerCount = 1,
      .ppEnabledLayerNames = &val_layers
    };
    if ((result = vkCreateDevice(phy_device, &dev_create_info, NULL, &device))) goto error;
    vkGetDeviceQueue(device, 0, 0, &qraphics);
    vkGetDeviceQueue(device, 0, 0, &qresent);
  }
  printf("Dev  created\n");
  uint32_t image_count = 0;
  VkImage* images = NULL;
  VkExtent2D extent;
  VkSwapchainKHR swapchain = {0};
  {
    VkSurfaceCapabilitiesKHR capab = {0};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phy_device, surface, &capab);

    extent = capab.maxImageExtent;
    VkSwapchainCreateInfoKHR info = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .imageExtent = extent,
      .surface = surface,
      .minImageCount = 3,
      .imageFormat = VK_FORMAT_B8G8R8A8_SRGB,
      .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices = &(uint32_t){0},
      .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = VK_PRESENT_MODE_MAILBOX_KHR,
      .clipped = VK_TRUE,
      .oldSwapchain = VK_NULL_HANDLE
    };
    if (VK_SUCCESS != (result = vkCreateSwapchainKHR(device, &info, NULL, &swapchain))) goto error;

    vkGetSwapchainImagesKHR(device, swapchain, &image_count, NULL);
    images = malloc(sizeof(VkImage) * image_count);
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, images);
  }
  printf("Swapchains and %d images created\n", image_count);
  VkImageView* image_views = malloc(sizeof(VkImageView) * image_count);
  {
    for (uint32_t i = 0; i < image_count; ++i) {
      VkImageViewCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = images[i],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_B8G8R8A8_SRGB,
        .subresourceRange = {
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1
        }
      };
      if ((result = vkCreateImageView(device, &info, 0, &image_views[i]))) goto error;
    }
  }
  printf("Image views created\n");
  VkRenderPass render_pass = {0};
  {
    VkRenderPassCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &(VkAttachmentDescription) {
        .format = VK_FORMAT_B8G8R8A8_SRGB,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
      },
      .subpassCount = 1,
      .pSubpasses = &(VkSubpassDescription) {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &(VkAttachmentReference) {
          .attachment = 0,
          .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        }
      },
      .dependencyCount = 1,
      .pDependencies = &(VkSubpassDependency) {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
      }
    };
    if ((result = vkCreateRenderPass(device, &info, NULL, &render_pass))) goto error;
  }
  VkPipeline pipeline = {0};
  {
    VkShaderModule vert, frag;
    VkPipelineLayout pipelineLayout;
    {
      VkPipelineLayoutCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 0,
        .pushConstantRangeCount = 0
      };
      if ((result = vkCreatePipelineLayout(device, &info, NULL, &pipelineLayout))) goto error;
    }
    if ((result = load_shader(device, "vert.spv", &vert))) goto error;
    if ((result = load_shader(device, "frag.spv", &frag))) goto error;
    VkGraphicsPipelineCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = 2,
      .pStages = (VkPipelineShaderStageCreateInfo[]) { {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vert,
        .pName = "main"
      }, {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = frag,
        .pName = "main"
      } },
      .pVertexInputState = &(VkPipelineVertexInputStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      },
      .pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
      },
      .pViewportState = &(VkPipelineViewportStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1
      },
      .pRasterizationState = &(VkPipelineRasterizationStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE
      },
      .pMultisampleState = &(VkPipelineMultisampleStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
      },
      .pColorBlendState = &(VkPipelineColorBlendStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &(VkPipelineColorBlendAttachmentState) {
          .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
          .blendEnable = VK_FALSE
        },
      },
      .pDynamicState = &(VkPipelineDynamicStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = (VkDynamicState[]) { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR },
      },
      .layout = pipelineLayout,
      .renderPass = render_pass,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE
    };
    if ((result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, NULL, &pipeline))) goto error;
    vkDestroyShaderModule(device, vert, NULL);
    vkDestroyShaderModule(device, frag, NULL);
  }
  printf("Render pass created\n");
  VkFramebuffer* framebuffers = malloc(sizeof(VkFramebuffer) * image_count);
  {
    for (uint32_t i = 0; i < image_count; ++i) {
      VkFramebufferCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = render_pass,
        .attachmentCount = 1,
        .pAttachments = &image_views[i],
        .width = extent.width,
        .height = extent.height,
        .layers = 1
      };
      if ((result = vkCreateFramebuffer(device, &info, NULL, &framebuffers[i]))) goto error;
    }
  }
  printf("Frambuffers created\n");
  VkCommandPool command_pool;
  {
    VkCommandPoolCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = 0,
    };
    vkCreateCommandPool(device, &info, NULL, &command_pool);
  }
  printf("Command pool created\n");
  VkCommandBuffer command_buffer;
  {
    VkCommandBufferAllocateInfo info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = command_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1
    };
    vkAllocateCommandBuffers(device, &info, &command_buffer);
  }
  printf("Command buffer created\n");
  VkSemaphore sem_image_available, sem_render_finished;
  VkFence fence;
  {
    VkSemaphoreCreateInfo s_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo f_info = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };
    if ((result = vkCreateSemaphore(device, &s_info, NULL, &sem_image_available))) goto error;
    if ((result = vkCreateSemaphore(device, &s_info, NULL, &sem_render_finished))) goto error;
    if ((result = vkCreateFence(device, &f_info, NULL, &fence))) goto error;
  }
  printf("Semaphores created\n");

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &fence);
    uint32_t imageIndex;
    vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, sem_image_available, VK_NULL_HANDLE, &imageIndex);
    vkResetCommandBuffer(command_buffer, 0);
    {
      VkCommandBufferBeginInfo beginInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
      if ((result = vkBeginCommandBuffer(command_buffer, &beginInfo))) goto error;
      VkRenderPassBeginInfo rp_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = render_pass,
        .framebuffer = framebuffers[imageIndex],
        .renderArea.extent = extent,
        .clearValueCount = 1,
        .pClearValues = &(VkClearValue) { 0.0f, 0.0f, 0.0f, 1.0f }
      };
      vkCmdBeginRenderPass(command_buffer, &rp_info, VK_SUBPASS_CONTENTS_INLINE);
      vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
      vkCmdSetViewport(command_buffer, 0, 1, &(VkViewport){ .width = extent.width/2.f, .height = extent.height/2.f, .maxDepth = 1.0f });
      vkCmdSetScissor(command_buffer, 0, 1, &(VkRect2D) { .extent = extent });
      vkCmdDraw(command_buffer, 3, 1, 0, 0);
      static float x = 0.f;
      x += 0.1f;
      vkCmdSetViewport(command_buffer, 0, 1, &(VkViewport){ .x = x, .width = extent.width/2.f, .height = extent.height/2.f, .maxDepth = 1.0f });
      vkCmdDraw(command_buffer, 3, 1, 0, 0);
      vkCmdSetViewport(command_buffer, 0, 1, &(VkViewport){ .y = x, .width = extent.width/2.f, .height = extent.height/2.f, .maxDepth = 1.0f });
      vkCmdDraw(command_buffer, 3, 1, 0, 0);
      vkCmdEndRenderPass(command_buffer);
      if ((result = vkEndCommandBuffer(command_buffer))) goto error;
    }
    {
      VkSubmitInfo info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &sem_image_available,
        .pWaitDstStageMask = &(VkPipelineStageFlags) {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &sem_render_finished
      };
      if ((result = vkQueueSubmit(qraphics, 1, &info, fence))) goto error;
    }
    {
      VkPresentInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &sem_render_finished,
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &imageIndex,
      };
      vkQueuePresentKHR(qresent, &info);
    }
  }

  return 0;

error:
  printf("ERROR: %d\n", result);
  return 1;
}
// glslc -o frag.spv frag.frag
// glslc -o vert.spv vert.vert
// gcc -o vk -Wall -Wextra -ggdb main_vk.c -lvulkan -lglfw && ./vk
