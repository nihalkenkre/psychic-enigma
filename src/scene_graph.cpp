#include "scene_graph.hpp"
#include "vulkan_objects.hpp"

#include <vector>
#include <array>
#include <imgui.h>
#include <imnodes.h>
#include <vulkan/vulkan_core.h>

struct BasicShapeNode
{
	ImVec2 position;
	int node_id;
	int input_attr_id;
	int output_attr_id;
	std::vector<const char*> selectables;
	size_t selected_idx;

	struct PushConstantRange {

	};

	VkPipeline pipeline;

	// Desc Heap
	struct DescHeap {
		Buffer buffer;
	};

	// Desc Buffer
	struct DescBuffer {
		Buffer buffer;
		VkDescriptorSetLayout desc_set_layout;
		VkPipelineLayout pipeline_layout;
		VkDeviceSize desc_set_layout_size;
		std::array<VkDeviceSize, 3> desc_binding_offsets;
	};
};

static std::vector<BasicShapeNode> basic_shape_nodes;
static std::pair<int, int> links;

static int imnodes_id = 0;

void SceneGraph_Create()
{

}

void SceneGraph_AddBasicShapeNode(ImVec2 position)
{
	basic_shape_nodes.push_back(
		BasicShapeNode{
			.position = position,
			.node_id = imnodes_id++,
			.input_attr_id = imnodes_id++,
			.output_attr_id = imnodes_id++,
			.selectables = std::vector{"Cube", "Sphere"},
			.selected_idx = 0
		}
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
		if (ImGui::BeginCombo("Shapes", bsn.selectables.at(bsn.selected_idx)))
		{
			for (size_t s = 0; s < bsn.selectables.size(); ++s)
			{
				const bool is_selected = (bsn.selected_idx == s);

				if (ImGui::Selectable(bsn.selectables.at(s), is_selected))
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

void SceneGraph_Destroy()
{
}
