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
  try {
    const auto pixels = LoadImagePixels(imagePath);
    UploadTexture(pixels, ctx.physicalDevice, ctx.graphicsQueue,
                  ctx.queueFamily);
    CreateDescriptors();
    CreatePipeline(ctx.swapchainFormat);
  } catch (...) {
    Destroy();
    throw;
  }
}

ImageLayer::~ImageLayer() {
  Destroy();
}

void ImageLayer::Render(VkCommandBuffer cmd, VkExtent2D extent) const {
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_,
                          0, 1, &descriptorSet_, 0, nullptr);

  struct PushConstants {
    float imageAspect;
    float screenAspect;
  };
  const PushConstants pc{
      static_cast<float>(imageWidth_) / static_cast<float>(imageHeight_),
      static_cast<float>(extent.width) / static_cast<float>(extent.height),
  };
  vkCmdPushConstants(cmd, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                     sizeof(pc), &pc);

  vkCmdDraw(cmd, 6, 1, 0, 0);
}

std::vector<uint8_t> ImageLayer::LoadImagePixels(const std::filesystem::path& path) {
  auto inp = OIIO::ImageInput::open(path.string());
  if (!inp)
    throw std::runtime_error(fmt::format("Failed to open image: {} ({})", path.string(), OIIO::geterror()));

  const OIIO::ImageSpec& spec = inp->spec();
  imageWidth_ = spec.width;
  imageHeight_ = spec.height;

  const size_t nchans = spec.nchannels;
  const size_t npixels = static_cast<size_t>(imageWidth_) * imageHeight_;

  std::vector<uint8_t> raw(npixels * nchans);
  if (!inp->read_image(0, 0, 0, nchans, OIIO::TypeDesc::UINT8, raw.data()))
    throw std::runtime_error(fmt::format("Failed to read image pixels: {} ({})", path.string(), inp->geterror()));
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

  VkBuffer stagingBuffer = VK_NULL_HANDLE;
  VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
  VkCommandPool uploadPool = VK_NULL_HANDLE;

  try {
    {
      VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
      bci.size = dataSize;
      bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
      bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      VK_CHECK(vkCreateBuffer(device_, &bci, nullptr, &stagingBuffer));

      VkMemoryRequirements reqs{};
      vkGetBufferMemoryRequirements(device_, stagingBuffer, &reqs);

      VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
      ai.allocationSize = reqs.size;
      ai.memoryTypeIndex =
          FindMemoryType(memProps, reqs.memoryTypeBits,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
      VK_CHECK(vkAllocateMemory(device_, &ai, nullptr, &stagingMemory));
      VK_CHECK(vkBindBufferMemory(device_, stagingBuffer, stagingMemory, 0));

      void* mapped = nullptr;
      VK_CHECK(vkMapMemory(device_, stagingMemory, 0, dataSize, 0, &mapped));
      std::memcpy(mapped, pixels.data(), static_cast<size_t>(dataSize));
      vkUnmapMemory(device_, stagingMemory);
    }

    {
      VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
      ici.imageType = VK_IMAGE_TYPE_2D;
      ici.format = VK_FORMAT_R8G8B8A8_UNORM;
      ici.extent = {static_cast<uint32_t>(imageWidth_),
                    static_cast<uint32_t>(imageHeight_), 1};
      ici.mipLevels = 1;
      ici.arrayLayers = 1;
      ici.samples = VK_SAMPLE_COUNT_1_BIT;
      ici.tiling = VK_IMAGE_TILING_OPTIMAL;
      ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
      ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      VK_CHECK(vkCreateImage(device_, &ici, nullptr, &texture_));

      VkMemoryRequirements reqs{};
      vkGetImageMemoryRequirements(device_, texture_, &reqs);

      VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
      ai.allocationSize = reqs.size;
      ai.memoryTypeIndex = FindMemoryType(memProps, reqs.memoryTypeBits,
                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
      VK_CHECK(vkAllocateMemory(device_, &ai, nullptr, &textureMemory_));
      VK_CHECK(vkBindImageMemory(device_, texture_, textureMemory_, 0));
    }

    {
      VkCommandPoolCreateInfo cpci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
      cpci.queueFamilyIndex = queueFamily;
      cpci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
      VK_CHECK(vkCreateCommandPool(device_, &cpci, nullptr, &uploadPool));

      VkCommandBuffer uploadCmd = VK_NULL_HANDLE;
      VkCommandBufferAllocateInfo cbai{
          VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
      cbai.commandPool = uploadPool;
      cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      cbai.commandBufferCount = 1;
      VK_CHECK(vkAllocateCommandBuffers(device_, &cbai, &uploadCmd));

      VkCommandBufferBeginInfo beginInfo{
          VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
      beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      VK_CHECK(vkBeginCommandBuffer(uploadCmd, &beginInfo));

      VkImageMemoryBarrier toTransfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
      toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      toTransfer.image = texture_;
      toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      toTransfer.subresourceRange.levelCount = 1;
      toTransfer.subresourceRange.layerCount = 1;
      toTransfer.srcAccessMask = 0;
      toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      vkCmdPipelineBarrier(uploadCmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                           nullptr, 1, &toTransfer);

      VkBufferImageCopy region{};
      region.bufferOffset = 0;
      region.bufferRowLength = 0;
      region.bufferImageHeight = 0;
      region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      region.imageSubresource.mipLevel = 0;
      region.imageSubresource.baseArrayLayer = 0;
      region.imageSubresource.layerCount = 1;
      region.imageOffset = {0, 0, 0};
      region.imageExtent = {static_cast<uint32_t>(imageWidth_),
                            static_cast<uint32_t>(imageHeight_), 1};
      vkCmdCopyBufferToImage(uploadCmd, stagingBuffer, texture_,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

      VkImageMemoryBarrier toShader{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
      toShader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      toShader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      toShader.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      toShader.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      toShader.image = texture_;
      toShader.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      toShader.subresourceRange.levelCount = 1;
      toShader.subresourceRange.layerCount = 1;
      toShader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      toShader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      vkCmdPipelineBarrier(uploadCmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                           0, nullptr, 1, &toShader);

      VK_CHECK(vkEndCommandBuffer(uploadCmd));

      VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
      si.commandBufferCount = 1;
      si.pCommandBuffers = &uploadCmd;
      VK_CHECK(vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE));
      VK_CHECK(vkQueueWaitIdle(queue));
    }

    vkDestroyCommandPool(device_, uploadPool, nullptr);
    uploadPool = VK_NULL_HANDLE;

    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    stagingBuffer = VK_NULL_HANDLE;
    vkFreeMemory(device_, stagingMemory, nullptr);
    stagingMemory = VK_NULL_HANDLE;

    {
      VkImageViewCreateInfo ivci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
      ivci.image = texture_;
      ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
      ivci.format = VK_FORMAT_R8G8B8A8_UNORM;
      ivci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
      ivci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      ivci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      ivci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
      ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      ivci.subresourceRange.levelCount = 1;
      ivci.subresourceRange.layerCount = 1;
      VK_CHECK(vkCreateImageView(device_, &ivci, nullptr, &textureView_));
    }

    {
      VkSamplerCreateInfo sci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
      sci.magFilter = VK_FILTER_LINEAR;
      sci.minFilter = VK_FILTER_LINEAR;
      sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      sci.anisotropyEnable = VK_FALSE;
      sci.maxAnisotropy = 1.0f;
      sci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
      sci.unnormalizedCoordinates = VK_FALSE;
      sci.compareEnable = VK_FALSE;
      sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
      VK_CHECK(vkCreateSampler(device_, &sci, nullptr, &sampler_));
    }

  } catch (...) {
    vkDestroyCommandPool(device_, uploadPool, nullptr);
    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, stagingMemory, nullptr);
    throw;
  }
}

void ImageLayer::CreateDescriptors() {
  VkDescriptorSetLayoutBinding binding{};
  binding.binding = 0;
  binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  binding.descriptorCount = 1;
  binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo dslci{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  dslci.bindingCount = 1;
  dslci.pBindings = &binding;
  VK_CHECK(vkCreateDescriptorSetLayout(device_, &dslci, nullptr,
                                       &descriptorSetLayout_));

  VkDescriptorPoolSize poolSize{};
  poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSize.descriptorCount = 1;

  VkDescriptorPoolCreateInfo dpci{
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  dpci.maxSets = 1;
  dpci.poolSizeCount = 1;
  dpci.pPoolSizes = &poolSize;
  VK_CHECK(vkCreateDescriptorPool(device_, &dpci, nullptr, &descriptorPool_));

  VkDescriptorSetAllocateInfo dsai{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  dsai.descriptorPool = descriptorPool_;
  dsai.descriptorSetCount = 1;
  dsai.pSetLayouts = &descriptorSetLayout_;
  VK_CHECK(vkAllocateDescriptorSets(device_, &dsai, &descriptorSet_));

  VkDescriptorImageInfo imgInfo{};
  imgInfo.sampler = sampler_;
  imgInfo.imageView = textureView_;
  imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
  write.dstSet = descriptorSet_;
  write.dstBinding = 0;
  write.dstArrayElement = 0;
  write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  write.descriptorCount = 1;
  write.pImageInfo = &imgInfo;
  vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
}

void ImageLayer::CreatePipeline(VkFormat swapchainFormat) {
  ShaderModule vertModule(device_, SHADER_DIR "/image.vert.spv");
  ShaderModule fragModule(device_, SHADER_DIR "/image.frag.spv");

  VkPipelineShaderStageCreateInfo stages[2]{};
  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = vertModule.get();
  stages[0].pName = "main";
  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = fragModule.get();
  stages[1].pName = "main";

    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pcRange.offset = 0;
    pcRange.size = 2 * sizeof(float);

    VkPipelineLayoutCreateInfo layoutCI{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutCI.setLayoutCount = 1;
    layoutCI.pSetLayouts = &descriptorSetLayout_;
    layoutCI.pushConstantRangeCount = 1;
    layoutCI.pPushConstantRanges = &pcRange;
    VK_CHECK(
        vkCreatePipelineLayout(device_, &layoutCI, nullptr, &pipelineLayout_));

    VkPipelineVertexInputStateCreateInfo vertexInput{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlend{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &colorBlendAttachment;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                      VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkPipelineRenderingCreateInfo renderingCI{
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    renderingCI.colorAttachmentCount = 1;
    renderingCI.pColorAttachmentFormats = &swapchainFormat;

    VkGraphicsPipelineCreateInfo pipelineCI{
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineCI.pNext = &renderingCI;
    pipelineCI.stageCount = 2;
    pipelineCI.pStages = stages;
    pipelineCI.pVertexInputState = &vertexInput;
    pipelineCI.pInputAssemblyState = &inputAssembly;
    pipelineCI.pViewportState = &viewportState;
    pipelineCI.pRasterizationState = &rasterizer;
    pipelineCI.pMultisampleState = &multisample;
    pipelineCI.pColorBlendState = &colorBlend;
    pipelineCI.pDynamicState = &dynamicState;
    pipelineCI.layout = pipelineLayout_;
    pipelineCI.renderPass = VK_NULL_HANDLE;
    VK_CHECK(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineCI,
                                       nullptr, &pipeline_));
}

void ImageLayer::Destroy() {
  if (device_ == VK_NULL_HANDLE) return;

  vkDestroyPipeline(device_, pipeline_, nullptr);
  pipeline_ = VK_NULL_HANDLE;

  vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
  pipelineLayout_ = VK_NULL_HANDLE;

  // Destroying the pool implicitly frees the descriptor set.
  vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
  descriptorPool_ = VK_NULL_HANDLE;
  descriptorSet_ = VK_NULL_HANDLE;

  vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);
  descriptorSetLayout_ = VK_NULL_HANDLE;

  vkDestroySampler(device_, sampler_, nullptr);
  sampler_ = VK_NULL_HANDLE;

  vkDestroyImageView(device_, textureView_, nullptr);
  textureView_ = VK_NULL_HANDLE;

  vkDestroyImage(device_, texture_, nullptr);
  texture_ = VK_NULL_HANDLE;

  vkFreeMemory(device_, textureMemory_, nullptr);
  textureMemory_ = VK_NULL_HANDLE;

  device_ = VK_NULL_HANDLE;
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
