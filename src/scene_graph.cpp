#include "scene_graph.hpp"

#include <vector>
#include <imgui.h>
#include <imnodes.h>

struct BasicShapeNode
{
	ImVec2 position;
	int id;
};

static std::vector<BasicShapeNode> basic_shape_nodes;
static std::pair<int, int> links;

static int basic_shape_node_id = 0;

void SceneGraph_Create()
{}

void SceneGraph_Render()
{
	for (const auto bsn : basic_shape_nodes)
	{
		ImNodes::BeginNode(bsn.id);
		ImNodes::BeginNodeTitleBar();
		ImGui::Text("Basic Shapes");
		ImNodes::EndNodeTitleBar();

		if (ImGui::BeginCombo("Shapes", "Box"))
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
		ImNodes::BeginOutputAttribute(1);
		ImGui::Text("Mesh");
		ImNodes::EndOutputAttribute();
		ImNodes::EndNode();
	}
}

void SceneGraph_AddBasicShapeNode(ImVec2 position)
{
	basic_shape_nodes.push_back(BasicShapeNode{ .position = position, .id = basic_shape_node_id++ });
}

void SceneGraph_ConnectAttributes()
{}

void SceneGraph_DisconnectAttributes()
{}

void SceneGraph_Destroy()
{}
