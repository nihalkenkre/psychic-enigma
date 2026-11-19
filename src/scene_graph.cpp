#include "scene_graph.hpp"
#include "vulkan_objects.hpp"

#include <vector>
#include <array>
#include <imgui.h>
#include <imnodes.h>
#include <vulkan/vulkan_core.h>

struct BasicShapesNode
{
	struct GraphicsPipelineData {
		struct PushConstantRange {
			
		};
		
		VkPipeline pipeline;
		
		struct DescHeap {
			Buffer buffer;
		};

		struct DescBuffer {
			Buffer buffer;
			VkPipelineLayout pipeline_layout;
		};

		union {
			DescHeap desc_heap;
			DescBuffer desc_buffer;
		};
	};

	struct ComputePipelineData {
		struct PushConstantRange {

		};

		VkPipeline pipeline;

		struct DescHeap {
			Buffer buffer;
		};

		struct DescBuffer {
			Buffer buffer;
			VkPipelineLayout pipeline_layout;
		};

		union {
			DescHeap desc_heap;
			DescBuffer desc_buffer;
		};
	};

	ImVec2 position;
	int node_id;
	int input_attr_id;
	int output_attr_id;
	std::vector<const char*> shape_names;
	size_t selected_idx;

	GraphicsPipelineData graphics_pipeline_data;
	ComputePipelineData compute_pipeline_data;
};

namespace {
	std::vector<BasicShapesNode> basic_shape_nodes;
	std::pair<int, int> links;

	int imnodes_id = 0;
}

void SceneGraph_Create()
{

}

VkPipeline SceneGraph_BasicShapesNode_GraphicsPipeline_Create()
{
	VkShaderModule vertex_shader_module = ShaderModule_Create("shaders\\glsl\\basic_shapes_node.vert.glsl.spv");
	VkShaderModule fragment_shader_module = ShaderModule_Create("shaders\\glsl\\basic_shapes_node.frag.glsl.spv");
	
	vkDestroyShaderModule(Device_Device(), vertex_shader_module, nullptr);
	vkDestroyShaderModule(Device_Device(), fragment_shader_module, nullptr);
	
	return VK_NULL_HANDLE;
}

BasicShapesNode SceneGraph_BasicShapesNode_Create(ImVec2 position)
{
	BasicShapesNode bsn = {
		.position = position,
		.node_id = imnodes_id++,
		.input_attr_id = imnodes_id++,
		.output_attr_id = imnodes_id++,
		.shape_names = std::vector{"Cube", "Sphere"},
		.selected_idx = 0
	};

	return bsn;
}

void SceneGraph_AddBasicShapeNode(ImVec2 position)
{
	basic_shape_nodes.push_back(
		//BasicShapesNode{
		//	.position = position,
		//	.node_id = imnodes_id++,
		//	.input_attr_id = imnodes_id++,
		//	.output_attr_id = imnodes_id++,
		//	.shape_names = std::vector{"Cube", "Sphere"},
		//	.selected_idx = 0
		//}

		SceneGraph_BasicShapesNode_Create(position)
	);
}

void SceneGraph_Display()
{
	for (auto& bsn : basic_shape_nodes)
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
	for (const auto& bsn : basic_shape_nodes)
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

void SceneGraph_BasicShapesNodes_Destroy()
{
	for (const auto& bsn : basic_shape_nodes)
	{
		vkDestroyPipeline(Device_Device(), bsn.graphics_pipeline_data.pipeline, nullptr);
		vkDestroyPipeline(Device_Device(), bsn.compute_pipeline_data.pipeline, nullptr);
	}
}

void SceneGraph_Destroy()
{
}
