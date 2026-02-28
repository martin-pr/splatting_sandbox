#include "TriangleLayer.h"

#include <fmt/core.h>

#include <fstream>
#include <stdexcept>

#include "VulkanErrors.h"

#ifndef SHADER_DIR
#define SHADER_DIR "shaders"
#endif

TriangleLayer::TriangleLayer(const Renderer::Context& ctx)
    : device_(ctx.device), swapchainFormat_(ctx.swapchainFormat) {
  VkShaderModule vertModule = VK_NULL_HANDLE;
  VkShaderModule fragModule = VK_NULL_HANDLE;
  try {
    const auto vertWords = LoadSpirvWords(SHADER_DIR "/triangle.vert.spv");
    const auto fragWords = LoadSpirvWords(SHADER_DIR "/triangle.frag.spv");

    VkShaderModuleCreateInfo vertCI{
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    vertCI.codeSize = vertWords.size() * sizeof(uint32_t);
    vertCI.pCode = vertWords.data();
    VK_CHECK(vkCreateShaderModule(device_, &vertCI, nullptr, &vertModule));

    VkShaderModuleCreateInfo fragCI{
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
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

    VkPipelineLayoutCreateInfo layoutCI{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    VK_CHECK(
        vkCreatePipelineLayout(device_, &layoutCI, nullptr, &pipelineLayout_));

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

std::vector<uint32_t> TriangleLayer::LoadSpirvWords(const char* path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file)
    throw std::runtime_error(fmt::format("Failed to open SPIR-V file: {}", path));

  const std::streamsize size = file.tellg();
  if (size <= 0 || (size % 4) != 0)
    throw std::runtime_error(fmt::format("Invalid SPIR-V file size: {}", path));
  file.seekg(0, std::ios::beg);

  std::vector<uint32_t> outWords;
  outWords.resize(size / sizeof(uint32_t));
  if (!file.read(reinterpret_cast<char*>(outWords.data()), size))
    throw std::runtime_error(fmt::format("Failed to read SPIR-V file: {}", path));

  return outWords;
}

void TriangleLayer::Destroy() {
  if (device_ == VK_NULL_HANDLE) return;
  vkDestroyPipeline(device_, pipeline_, nullptr);
  vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
  pipeline_ = VK_NULL_HANDLE;
  pipelineLayout_ = VK_NULL_HANDLE;
  device_ = VK_NULL_HANDLE;
}
