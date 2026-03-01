#include "ImageLayer.h"

#include <OpenImageIO/imageio.h>
#include <fmt/core.h>

#include <cstring>
#include <filesystem>
#include <stdexcept>

#include "VulkanErrors.h"
#include "VulkanShaders.h"

#ifndef SHADER_DIR
#define SHADER_DIR "shaders"
#endif

ImageLayer::ImageLayer(const Renderer::Context& ctx,
                       const std::filesystem::path& imagePath)
    : PipelineLayerBase(ctx) {
  const auto pixels = LoadImagePixels(imagePath);
  UploadTexture(pixels, ctx.physicalDevice, ctx.graphicsQueue, ctx.queueFamily);
  CreateDescriptors();
  CreatePipeline(ctx.swapchainFormat);
}

void ImageLayer::Render(VkCommandBuffer cmd, VkExtent2D extent) const {
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.get());
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipelineLayout_.get(), 0, 1, &descriptorSet_, 0,
                          nullptr);

  struct PushConstants {
    float imageAspect;
    float screenAspect;
  };
  const PushConstants pc{
      static_cast<float>(imageWidth_) / static_cast<float>(imageHeight_),
      static_cast<float>(extent.width) / static_cast<float>(extent.height),
  };
  vkCmdPushConstants(cmd, pipelineLayout_.get(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                     sizeof(pc), &pc);

  vkCmdDraw(cmd, 6, 1, 0, 0);
}

std::vector<uint8_t> ImageLayer::LoadImagePixels(
    const std::filesystem::path& path) {
  auto inp = OIIO::ImageInput::open(path.string());
  if (!inp)
    throw std::runtime_error(fmt::format("Failed to open image: {} ({})",
                                         path.string(), OIIO::geterror()));

  const OIIO::ImageSpec& spec = inp->spec();
  imageWidth_ = spec.width;
  imageHeight_ = spec.height;

  const size_t nchans = spec.nchannels;
  const size_t npixels = static_cast<size_t>(imageWidth_) * imageHeight_;

  std::vector<uint8_t> raw(npixels * nchans);
  if (!inp->read_image(0, 0, 0, nchans, OIIO::TypeDesc::UINT8, raw.data()))
    throw std::runtime_error(fmt::format("Failed to read image pixels: {} ({})",
                                         path.string(), inp->geterror()));
  inp->close();

  std::vector<uint8_t> pixels(npixels * 4);
  auto* ptr = raw.data();
  auto* pix = pixels.data();
  for (size_t i = 0; i < npixels; ++i) {
    for (std::size_t j = 0; j < 4u; ++j) {
      if (j < nchans) {
        (*pix++) = *ptr++;
      } else {
        (*pix++) = (j == 3u) ? 255 : 0;
      }
    }
  }
  return pixels;
}

void ImageLayer::UploadTexture(const std::vector<uint8_t>& pixels,
                               VkPhysicalDevice physicalDevice, VkQueue queue,
                               uint32_t queueFamily) {
  const VkDeviceSize dataSize = static_cast<VkDeviceSize>(imageWidth_) *
                                static_cast<VkDeviceSize>(imageHeight_) * 4;

  VkPhysicalDeviceMemoryProperties memProps{};
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

  // Staging buffer
  Buffer stagingBuffer(device_,
                       VkBufferCreateInfo{
                           .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                           .size = dataSize,
                           .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       });

  VkMemoryRequirements reqs{};
  vkGetBufferMemoryRequirements(device_, stagingBuffer.get(), &reqs);
  DeviceMemory stagingMemory(
      device_, VkMemoryAllocateInfo{
                   .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                   .allocationSize = reqs.size,
                   .memoryTypeIndex =
                       FindMemoryType(memProps, reqs.memoryTypeBits,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
               });
  VK_CHECK(
      vkBindBufferMemory(device_, stagingBuffer.get(), stagingMemory.get(), 0));

  {
    void* mapped = nullptr;
    VK_CHECK(
        vkMapMemory(device_, stagingMemory.get(), 0, dataSize, 0, &mapped));
    std::memcpy(mapped, pixels.data(), static_cast<size_t>(dataSize));
    vkUnmapMemory(device_, stagingMemory.get());
  }

  // Texture image
  {
    texture_ =
        Image(device_, VkImageCreateInfo{
                           .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                           .imageType = VK_IMAGE_TYPE_2D,
                           .format = VK_FORMAT_R8G8B8A8_UNORM,
                           .extent = {static_cast<uint32_t>(imageWidth_),
                                      static_cast<uint32_t>(imageHeight_), 1},
                           .mipLevels = 1,
                           .arrayLayers = 1,
                           .samples = VK_SAMPLE_COUNT_1_BIT,
                           .tiling = VK_IMAGE_TILING_OPTIMAL,
                           .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                    VK_IMAGE_USAGE_SAMPLED_BIT,
                       });

    VkMemoryRequirements reqs{};
    vkGetImageMemoryRequirements(device_, texture_.get(), &reqs);
    textureMemory_ = DeviceMemory(
        device_, VkMemoryAllocateInfo{
                     .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                     .allocationSize = reqs.size,
                     .memoryTypeIndex =
                         FindMemoryType(memProps, reqs.memoryTypeBits,
                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
                 });
    VK_CHECK(
        vkBindImageMemory(device_, texture_.get(), textureMemory_.get(), 0));
  }

  // Upload via one-shot command buffer
  {
    CommandPool uploadPool(
        device_, VkCommandPoolCreateInfo{
                     .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                     .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
                     .queueFamilyIndex = queueFamily,
                 });

    VkCommandBuffer uploadCmd = VK_NULL_HANDLE;
    const VkCommandBufferAllocateInfo cbai{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = uploadPool.get(),
        .commandBufferCount = 1,
    };
    VK_CHECK(vkAllocateCommandBuffers(device_, &cbai, &uploadCmd));

    const VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    VK_CHECK(vkBeginCommandBuffer(uploadCmd, &beginInfo));

    const VkImageMemoryBarrier toTransfer{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = texture_.get(),
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
    };
    vkCmdPipelineBarrier(uploadCmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &toTransfer);

    const VkBufferImageCopy region{
        .imageSubresource =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
            },
        .imageExtent = {static_cast<uint32_t>(imageWidth_),
                        static_cast<uint32_t>(imageHeight_), 1},
    };
    vkCmdCopyBufferToImage(uploadCmd, stagingBuffer.get(), texture_.get(),
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    const VkImageMemoryBarrier toShader{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = texture_.get(),
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
    };
    vkCmdPipelineBarrier(uploadCmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &toShader);

    VK_CHECK(vkEndCommandBuffer(uploadCmd));

    const VkSubmitInfo si{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &uploadCmd,
    };
    VK_CHECK(vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(queue));
  }

  // Image view
  textureView_ =
      ImageView(device_, VkImageViewCreateInfo{
                             .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                             .image = texture_.get(),
                             .viewType = VK_IMAGE_VIEW_TYPE_2D,
                             .format = VK_FORMAT_R8G8B8A8_UNORM,
                             .subresourceRange =
                                 {
                                     .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                     .levelCount = 1,
                                     .layerCount = 1,
                                 },
                         });

  // Sampler
  sampler_ = Sampler(device_,
                     VkSamplerCreateInfo{
                         .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                         .magFilter = VK_FILTER_LINEAR,
                         .minFilter = VK_FILTER_LINEAR,
                         .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                         .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                         .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                         .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                         .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
                     });
}

void ImageLayer::CreateDescriptors() {
  descriptorSetLayout_ = DescriptorSetLayout(
      device_, VkDescriptorSetLayoutBinding{
                   .binding = 0,
                   .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   .descriptorCount = 1,
                   .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
               });

  descriptorPool_ =
      DescriptorPool(device_, 1,
                     VkDescriptorPoolSize{
                         .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                         .descriptorCount = 1,
                     });

  const VkDescriptorSetLayout layoutHandle = descriptorSetLayout_.get();
  const VkDescriptorSetAllocateInfo dsai{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = descriptorPool_.get(),
      .descriptorSetCount = 1,
      .pSetLayouts = &layoutHandle,
  };
  VK_CHECK(vkAllocateDescriptorSets(device_, &dsai, &descriptorSet_));

  const VkDescriptorImageInfo imgInfo{
      .sampler = sampler_.get(),
      .imageView = textureView_.get(),
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };

  const VkWriteDescriptorSet write{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = descriptorSet_,
      .dstBinding = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .pImageInfo = &imgInfo,
  };
  vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
}

void ImageLayer::CreatePipeline(VkFormat swapchainFormat) {
  ShaderModule vertModule(device_, SHADER_DIR "/image.vert.spv");
  ShaderModule fragModule(device_, SHADER_DIR "/image.frag.spv");

  const VkPipelineShaderStageCreateInfo stages[2]{
      {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .module = vertModule.get(),
          .pName = "main",
      },
      {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = fragModule.get(),
          .pName = "main",
      },
  };

  const VkPushConstantRange pcRange{
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
      .size = 2 * sizeof(float),
  };

  const VkDescriptorSetLayout layoutHandle = descriptorSetLayout_.get();
  const VkPipelineLayoutCreateInfo layoutCI{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &layoutHandle,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &pcRange,
  };
  pipelineLayout_ = PipelineLayout(device_, layoutCI);

  const VkPipelineVertexInputStateCreateInfo vertexInput{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
  };

  const VkPipelineInputAssemblyStateCreateInfo inputAssembly{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
  };

  const VkPipelineViewportStateCreateInfo viewportState{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .scissorCount = 1,
  };

  const VkPipelineRasterizationStateCreateInfo rasterizer{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .lineWidth = 1.0f,
  };

  const VkPipelineMultisampleStateCreateInfo multisample{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
  };

  const VkPipelineColorBlendAttachmentState colorBlendAttachment{
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };

  const VkPipelineColorBlendStateCreateInfo colorBlend{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &colorBlendAttachment,
  };

  const VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                          VK_DYNAMIC_STATE_SCISSOR};
  const VkPipelineDynamicStateCreateInfo dynamicState{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = 2,
      .pDynamicStates = dynamicStates,
  };

  const VkPipelineRenderingCreateInfo renderingCI{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &swapchainFormat,
  };

  const VkGraphicsPipelineCreateInfo pipelineCI{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = &renderingCI,
      .stageCount = 2,
      .pStages = stages,
      .pVertexInputState = &vertexInput,
      .pInputAssemblyState = &inputAssembly,
      .pViewportState = &viewportState,
      .pRasterizationState = &rasterizer,
      .pMultisampleState = &multisample,
      .pColorBlendState = &colorBlend,
      .pDynamicState = &dynamicState,
      .layout = pipelineLayout_.get(),
  };
  pipeline_ = Pipeline(device_, pipelineCI);
}

uint32_t ImageLayer::FindMemoryType(VkPhysicalDeviceMemoryProperties memProps,
                                    uint32_t typeBits,
                                    VkMemoryPropertyFlags required) {
  for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
    if ((typeBits & (1u << i)) &&
        (memProps.memoryTypes[i].propertyFlags & required) == required) {
      return i;
    }
  }
  throw std::runtime_error("No suitable Vulkan memory type found");
}
