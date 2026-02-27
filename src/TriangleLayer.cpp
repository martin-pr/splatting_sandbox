#include "TriangleLayer.h"

#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>

#ifndef SHADER_DIR
#define SHADER_DIR "shaders"
#endif

namespace {
#define STRING_CASE(x) \
  case x:              \
    return #x

const char* VkResultToString(VkResult r) {
  switch (r) {
    STRING_CASE(VK_SUCCESS);
    STRING_CASE(VK_ERROR_OUT_OF_HOST_MEMORY);
    STRING_CASE(VK_ERROR_OUT_OF_DEVICE_MEMORY);
    STRING_CASE(VK_ERROR_INITIALIZATION_FAILED);
    STRING_CASE(VK_ERROR_DEVICE_LOST);
    STRING_CASE(VK_ERROR_EXTENSION_NOT_PRESENT);
    STRING_CASE(VK_ERROR_FEATURE_NOT_PRESENT);
    STRING_CASE(VK_ERROR_LAYER_NOT_PRESENT);
    STRING_CASE(VK_ERROR_INCOMPATIBLE_DRIVER);
    STRING_CASE(VK_ERROR_TOO_MANY_OBJECTS);
    STRING_CASE(VK_ERROR_SURFACE_LOST_KHR);
    STRING_CASE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
    STRING_CASE(VK_ERROR_OUT_OF_DATE_KHR);
    STRING_CASE(VK_SUBOPTIMAL_KHR);
    default:
      return "VK_UNKNOWN_ERROR";
  }
}
#undef STRING_CASE

void VkCheck(VkResult result, const char* file, int line) {
  if (result != VK_SUCCESS) {
    throw std::runtime_error(std::string("Vulkan error at ") + file + ":" +
                             std::to_string(line) + " => " +
                             VkResultToString(result) + " (" +
                             std::to_string(static_cast<int>(result)) + ")");
  }
}

#define VK_CHECK(x) VkCheck((x), __FILE__, __LINE__)
}  // namespace

TriangleLayer::TriangleLayer(const Renderer::Context& ctx)
    : device_(ctx.device), swapchainFormat_(ctx.swapchainFormat) {
  VkShaderModule vertModule = VK_NULL_HANDLE;
  VkShaderModule fragModule = VK_NULL_HANDLE;
  try {
    std::vector<uint32_t> vertWords;
    std::vector<uint32_t> fragWords;
    if (!LoadSpirvWords(SHADER_DIR "/triangle.vert.spv", vertWords) ||
        !LoadSpirvWords(SHADER_DIR "/triangle.frag.spv", fragWords)) {
      throw std::runtime_error("Failed to load triangle shader binaries");
    }

    VkShaderModuleCreateInfo vertCI{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    vertCI.codeSize = vertWords.size() * sizeof(uint32_t);
    vertCI.pCode = vertWords.data();
    VK_CHECK(vkCreateShaderModule(device_, &vertCI, nullptr, &vertModule));

    VkShaderModuleCreateInfo fragCI{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    fragCI.codeSize = fragWords.size() * sizeof(uint32_t);
    fragCI.pCode = fragWords.data();
    VK_CHECK(vkCreateShaderModule(device_, &fragCI, nullptr, &fragModule));

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertModule;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragModule;
    stages[1].pName = "main";

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
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                          VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT |
                                          VK_COLOR_COMPONENT_A_BIT;

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

    VkPipelineLayoutCreateInfo layoutCI{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    VK_CHECK(vkCreatePipelineLayout(device_, &layoutCI, nullptr,
                                    &pipelineLayout_));

    VkPipelineRenderingCreateInfo renderingCI{
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    renderingCI.colorAttachmentCount = 1;
    renderingCI.pColorAttachmentFormats = &swapchainFormat_;

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
    pipelineCI.subpass = 0;

    VK_CHECK(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineCI,
                                       nullptr, &pipeline_));

  } catch (...) {
    vkDestroyShaderModule(device_, fragModule, nullptr);
    vkDestroyShaderModule(device_, vertModule, nullptr);
    Destroy();
    throw;
  }
  vkDestroyShaderModule(device_, fragModule, nullptr);
  vkDestroyShaderModule(device_, vertModule, nullptr);
}

TriangleLayer::~TriangleLayer() {
  Destroy();
}

void TriangleLayer::Render(VkCommandBuffer cmd, VkExtent2D extent) const {
  VkViewport viewport{};
  viewport.width = static_cast<float>(extent.width);
  viewport.height = static_cast<float>(extent.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.extent = extent;
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
  vkCmdDraw(cmd, 3, 1, 0, 0);
}

bool TriangleLayer::LoadSpirvWords(const char* path,
                                   std::vector<uint32_t>& outWords) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) return false;

  const std::streamsize size = file.tellg();
  if (size <= 0 || (size % 4) != 0) return false;

  file.seekg(0, std::ios::beg);
  std::vector<char> bytes(static_cast<size_t>(size));
  if (!file.read(bytes.data(), size)) return false;

  outWords.resize(bytes.size() / sizeof(uint32_t));
  std::memcpy(outWords.data(), bytes.data(), bytes.size());
  return true;
}

void TriangleLayer::Destroy() {
  if (device_ == VK_NULL_HANDLE) return;
  vkDestroyPipeline(device_, pipeline_, nullptr);
  vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
  pipeline_ = VK_NULL_HANDLE;
  pipelineLayout_ = VK_NULL_HANDLE;
  device_ = VK_NULL_HANDLE;
}
