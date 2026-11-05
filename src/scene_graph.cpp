#include "scene_graph.hpp"

#include <vector>
#include <imgui.h>
#include <imnodes.h>

struct BasicShapeNode
{
	ImVec2 position;
	int node_id;
	int input_attr_id;
	int output_attr_id;
};

static std::vector<BasicShapeNode> basic_shape_nodes;
static std::pair<int, int> links;

static int imnodes_id = 0;

void SceneGraph_Create()
{
}

void SceneGraph_Render()
{
	for (const auto bsn : basic_shape_nodes)
	{
		ImNodes::BeginNode(bsn.node_id);
		ImNodes::BeginNodeTitleBar();
		ImGui::Text("Basic Shapes");
		ImNodes::EndNodeTitleBar();

		ImNodes::BeginInputAttribute(bsn.input_attr_id);
		ImGui::Text("UV");
		ImNodes::EndInputAttribute();

		ImGui::SetNextItemWidth(100);
		if (ImGui::BeginCombo("Shapes", "Cube"))
		{
			if (ImGui::Selectable("Cube"))
			{
				printf("render cube\n");
			}
			if (ImGui::Selectable("Sphere"))
			{
				printf("render sphere\n");
			}

			ImGui::EndCombo();
		}
		ImNodes::BeginOutputAttribute(bsn.output_attr_id);
		ImGui::Text("Mesh");
		ImNodes::EndOutputAttribute();
		ImNodes::EndNode();
	}
}

void SceneGraph_AddBasicShapeNode(ImVec2 position)
{
	basic_shape_nodes.push_back(
		BasicShapeNode{ 
			.position = position, 
			.node_id = imnodes_id++, 
			.input_attr_id = imnodes_id++, 
			.output_attr_id = imnodes_id++ 
		}
	);
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
