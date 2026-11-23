#include "scene_graph.hpp"
#include "vulkan_objects.hpp"
#include "utils.hpp"

#include <vector>
#include <array>
#include <imgui.h>
#include <imnodes.h>
#include <vulkan/vulkan_core.h>
#include <cglm/struct.h>

namespace
{
	struct Vertex
	{
		vec3 position;
		vec3 normal;
		vec2 uv_0;
	};

	struct CubeData
	{
		std::vector<Vertex> vertices = {
			Vertex{.position = {-0.5f, -0.5f, 0.5f}, .normal = {0.0f, 0.0f, 1.0f}, .uv_0 = {0.0f, 0.0f}},
			Vertex{.position = {0.5f, -0.5f, 0.5f}, .normal = {0.0f, 0.0f, 1.0f}, .uv_0 = {1.0f, 0.0f}},
			Vertex{.position = {0.5f, 0.5f, 0.5f}, .normal = {0.0f, 0.0f, 1.0f}, .uv_0 = {1.0f, 1.0f}},
			Vertex{.position = {-0.5f, 0.5f, 0.5f}, .normal = {0.0f, 0.0f, 1.0f}, .uv_0 = {0.0f, 1.0f}},

			Vertex{.position = {0.5f, -0.5f, -0.5f}, .normal = {0.0f, 0.0f, -1.0f}, .uv_0 = {0.0f, 0.0f}},
			Vertex{.position = {-0.5f, -0.5f, -0.5f}, .normal = {0.0f, 0.0f, -1.0f}, .uv_0 = {1.0f, 0.0f}},
			Vertex{.position = {-0.5f, 0.5f, -0.5f}, .normal = {0.0f, 0.0f, -1.0f}, .uv_0 = {1.0f, 1.0f}},
			Vertex{.position = {0.5f, 0.5f, -0.5f}, .normal = {0.0f, 0.0f, -1.0f}, .uv_0 = {0.0f, 1.0f}},

			Vertex{.position = {-0.5f, -0.5f, -0.5f}, .normal = {-1.0f, 0.0f, 0.0f}, .uv_0 = {0.0f, 0.0f}},
			Vertex{.position = {-0.5f, -0.5f, 0.5f}, .normal = {-1.0f, 0.0f, 0.0f}, .uv_0 = {1.0f, 0.0f}},
			Vertex{.position = {-0.5f, 0.5f, 0.5f}, .normal = {-1.0f, 0.0f, 0.0f}, .uv_0 = {1.0f, 1.0f}},
			Vertex{.position = {-0.5f, 0.5f, -0.5f}, .normal = {-1.0f, 0.0f, 0.0f}, .uv_0 = {0.0f, 1.0f}},

			Vertex{.position = {0.5f, -0.5f, 0.5f}, .normal = {1.0f, 0.0f, 0.0f}, .uv_0 = {0.0f, 0.0f}},
			Vertex{.position = {0.5f, -0.5f, -0.5f}, .normal = {1.0f, 0.0f, 0.0f}, .uv_0 = {1.0f, 0.0f}},
			Vertex{.position = {0.5f, 0.5f, -0.5f}, .normal = {1.0f, 0.0f, 0.0f}, .uv_0 = {1.0f, 1.0f}},
			Vertex{.position = {0.5f, 0.5f, 0.5f}, .normal = {1.0f, 0.0f, 0.0f}, .uv_0 = {0.0f, 1.0f}},

			Vertex{.position = {-0.5f, 0.5f, 0.5f}, .normal = {0.0f, 1.0f, 0.0f}, .uv_0 = {0.0f, 0.0f}},
			Vertex{.position = {0.5f, 0.5f, 0.5f}, .normal = {0.0f, 1.0f, 0.0f}, .uv_0 = {1.0f, 0.0f}},
			Vertex{.position = {0.5f, 0.5f, -0.5f}, .normal = {0.0f, 1.0f, 0.0f}, .uv_0 = {1.0f, 1.0f}},
			Vertex{.position = {-0.5f, 0.5f, -0.5f}, .normal = {0.0f, 1.0f, 0.0f}, .uv_0 = {0.0f, 1.0f}},

			Vertex{.position = {-0.5f, -0.5f, -0.5f}, .normal = {0.0f, -1.0f, 0.0f}, .uv_0 = {0.0f, 0.0f}},
			Vertex{.position = {0.5f, -0.5f, -0.5f}, .normal = {0.0f, -1.0f, 0.0f}, .uv_0 = {1.0f, 0.0f}},
			Vertex{.position = {0.5f, -0.5f, 0.5f}, .normal = {0.0f, -1.0f, 0.0f}, .uv_0 = {1.0f, 1.0f}},
			Vertex{.position = {-0.5f, -0.5f, 0.5f}, .normal = {0.0f, -1.0f, 0.0f}, .uv_0 = {0.0f, 1.0f}}};

		std::vector<uint32_t> indices = {
			0, 1, 2, 2, 3, 0,
			4, 5, 6, 6, 7, 4,
			8, 9, 10, 10, 11, 8,
			12, 13, 14, 14, 15, 12,
			16, 17, 18, 18, 19, 16,
			20, 21, 22, 22, 23, 20
		};
	};

	struct BasicShapesNodeVulkanObjects
	{
		struct GraphicsPipelineData
		{
			struct PushConstantRange
			{
				uint64_t mvp_ptr;
			};

			VkPipeline pipeline;

			struct DescHeap
			{
				Buffer buffer;
			};

			struct DescBuffer
			{
				Buffer buffer;
				VkPipelineLayout pipeline_layout;
			};

			union
			{
				DescHeap desc_heap;
				DescBuffer desc_buffer;
			};
		};

		struct ComputePipelineData
		{
			struct PushConstantRange
			{
			};

			VkPipeline pipeline;

			struct DescHeap
			{
				Buffer buffer;
			};

			struct DescBuffer
			{
				Buffer buffer;
				VkPipelineLayout pipeline_layout;
			};

			union
			{
				DescHeap desc_heap;
				DescBuffer desc_buffer;
			};
		};

		GraphicsPipelineData graphics_pipeline_data;
		ComputePipelineData compute_pipeline_data;
	};

	struct BasicShapesNode
	{
		ImVec2 position;
		int node_id;
		int input_attr_id;
		int output_attr_id;
		std::vector<const char *> shape_names;
		size_t selected_idx;
	};

	BasicShapesNodeVulkanObjects bsn_vo;
	std::vector<BasicShapesNode> basic_shape_nodes;
	std::pair<int, int> links;

	int imnodes_id = 0;
}

void SceneGraph_BasicShapesNode_GraphicsPipeline_Create()
{
	VkShaderModule vertex_shader_module = ShaderModule_Create("shaders\\glsl\\basic_shapes_node.vert.glsl.spv");
	VkShaderModule fragment_shader_module = ShaderModule_Create("shaders\\glsl\\basic_shapes_node.frag.glsl.spv");

	VkDevice device = Device_Device();

	if (descriptor_mode == EDescriptorMode::DescHeap)
	{
	}
	else if (descriptor_mode == EDescriptorMode::DescBuffer)
	{
		const VkPushConstantRange pc_rng = {
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
			.size = sizeof(BasicShapesNodeVulkanObjects::GraphicsPipelineData::PushConstantRange)
		};

		const VkPipelineLayoutCreateInfo gfx_pl_ci = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pushConstantRangeCount = 1,
			.pPushConstantRanges = &pc_rng
		};

		VK_CHECK("create basic shapes node graphics pipeline layout", vkCreatePipelineLayout(device, &gfx_pl_ci, nullptr, &bsn_vo.graphics_pipeline_data.desc_buffer.pipeline_layout));

#ifdef _DEBUG
		Utils_SetObjectName(device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, reinterpret_cast<uint64_t>(bsn_vo.graphics_pipeline_data.desc_buffer.pipeline_layout), "BasicShapesNode graphics pipeline layout");
#endif
	}
	else if (descriptor_mode == EDescriptorMode::DescIndex)
	{
	}

	const VkPipelineShaderStageCreateInfo stages[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vertex_shader_module,
			.pName = "main",
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = fragment_shader_module,
			.pName = "main",
		}
	};

	constexpr VkVertexInputBindingDescription ibds[] = {
		{
			.binding = 0,
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
		}
	};

	constexpr VkVertexInputAttributeDescription viads[] = {
		{
			.location = 0,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = 0,
		},
		{
			.location = 1,
		 	.binding = 0,
		 	.format = VK_FORMAT_R32G32B32_SFLOAT,
		 	.offset = sizeof(vec3)
		},
		{
			.location = 2,
		 	.binding = 0,
		 	.format = VK_FORMAT_R32G32_SFLOAT,
		 	.offset = sizeof(vec3) + sizeof(vec3)
		}
	};

	const VkPipelineVertexInputStateCreateInfo vias = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = _countof(ibds),
		.pVertexBindingDescriptions = ibds,
		.vertexAttributeDescriptionCount = _countof(viads),
		.pVertexAttributeDescriptions = viads,
	};

	constexpr VkPipelineInputAssemblyStateCreateInfo ias = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	};

	const VkViewport viewports[] = {
		{},
	};

	const VkRect2D scissors[] = {
		{},
	};

	const VkPipelineViewportStateCreateInfo vs = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = _countof(viewports),
		.pViewports = viewports,
		.scissorCount = _countof(scissors),
		.pScissors = scissors,
	};

	constexpr VkPipelineRasterizationStateCreateInfo rs = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.lineWidth = 1.f,
	};

	constexpr VkPipelineMultisampleStateCreateInfo ms = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
	};

	constexpr VkPipelineDepthStencilStateCreateInfo dss = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
		.minDepthBounds = 0.f,
		.maxDepthBounds = 1.f,
	};

	constexpr VkDynamicState dynamic_states[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};

	const VkPipelineDynamicStateCreateInfo ds = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = _countof(dynamic_states),
		.pDynamicStates = dynamic_states
	};

	const VkGraphicsPipelineCreateInfo gfx_p_ci = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = _countof(stages),
		.pStages = stages,
		.pVertexInputState = &vias,
		.pInputAssemblyState = &ias,
		.pViewportState = &vs,
		.pRasterizationState = &rs,
		.pMultisampleState = &ms,
		.pDepthStencilState = &dss,
		.pDynamicState = &ds,
		.layout = bsn_vo.graphics_pipeline_data.desc_buffer.pipeline_layout
	};

	VK_CHECK("create basic shapes node graphics pipeline", vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gfx_p_ci, nullptr, &bsn_vo.graphics_pipeline_data.pipeline));

#ifdef _DEBUG
	Utils_SetObjectName(device, VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<uint64_t>(bsn_vo.graphics_pipeline_data.pipeline), "BasicShapesNode graphics pipeline");
#endif

	vkDestroyShaderModule(device, vertex_shader_module, nullptr);
	vkDestroyShaderModule(device, fragment_shader_module, nullptr);
}

void SceneGraph_Create()
{
	SceneGraph_BasicShapesNode_GraphicsPipeline_Create();
}

BasicShapesNode SceneGraph_BasicShapesNode_Create(ImVec2 position)
{
	BasicShapesNode bsn = {
		.position = position,
		.node_id = imnodes_id++,
		.input_attr_id = imnodes_id++,
		.output_attr_id = imnodes_id++,
		.shape_names = std::vector{"Cube", "Sphere"},
		.selected_idx = 0};

	return bsn;
}

void SceneGraph_AddBasicShapeNode(ImVec2 position)
{
	basic_shape_nodes.push_back(
		// BasicShapesNode{
		//	.position = position,
		//	.node_id = imnodes_id++,
		//	.input_attr_id = imnodes_id++,
		//	.output_attr_id = imnodes_id++,
		//	.shape_names = std::vector{"Cube", "Sphere"},
		//	.selected_idx = 0
		// }

		SceneGraph_BasicShapesNode_Create(position));
}

void SceneGraph_Display()
{
	for (auto &bsn : basic_shape_nodes)
	{
		ImNodes::BeginNode(bsn.node_id);
		ImNodes::BeginNodeTitleBar();
		ImGui::Text("Basic Shapes");
		ImNodes::EndNodeTitleBar();

		ImGui::SetNextItemWidth(100);
		if (ImGui::BeginCombo("Shapes", bsn.shape_names.at(bsn.selected_idx)))
		{
			for (size_t s = 0; s < bsn.shape_names.size(); ++s)
			{
				const bool is_selected = (bsn.selected_idx == s);

				if (ImGui::Selectable(bsn.shape_names.at(s), is_selected))
				{
					bsn.selected_idx = s;
				}

				if (is_selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}

			ImGui::EndCombo();
		}

		ImNodes::BeginOutputAttribute(bsn.output_attr_id);
		ImGui::Text("Mesh");
		ImNodes::EndOutputAttribute();
		ImNodes::EndNode();
	}
}

void SceneGraph_Render(VkCommandBuffer cmd_buff)
{
	for (const auto &bsn : basic_shape_nodes)
	{
		if (bsn.selected_idx == 0) // Cube
		{
		}
		else if (bsn.selected_idx == 1) // Sphere
		{
		}
	}
}

void SceneGraph_ConnectAttributes()
{
}

void SceneGraph_DisconnectAttributes()
{
}

void SceneGraph_BasicShapesNodesGraphicsPipeline_Destroy()
{
	vkDestroyPipeline(Device_Device(), bsn_vo.graphics_pipeline_data.pipeline, nullptr);

	if (descriptor_mode == EDescriptorMode::DescHeap)
	{
	}
	else if (descriptor_mode == EDescriptorMode::DescBuffer)
	{
		vkDestroyPipelineLayout(Device_Device(), bsn_vo.graphics_pipeline_data.desc_buffer.pipeline_layout, nullptr);
		Allocator_DestroyBuffer(bsn_vo.graphics_pipeline_data.desc_buffer.buffer);
	}
	else if (descriptor_mode == EDescriptorMode::DescIndex)
	{
	}
}

void SceneGraph_BasicShapesNodes_Destroy()
{
	SceneGraph_BasicShapesNodesGraphicsPipeline_Destroy();
}

void SceneGraph_Destroy()
{
	SceneGraph_BasicShapesNodesGraphicsPipeline_Destroy();
}
