#pragma once

#include <imgui.h>
#include <vulkan/vulkan_core.h>

void SceneGraph_Create();
void SceneGraph_AddBasicShapeNode(ImVec2 position);
void SceneGraph_Display();
void SceneGraph_Render(VkCommandBuffer cmd_buff);
void SceneGraph_ConnectAttributes();
void SceneGraph_DisconnectAttributes();
void SceneGraph_Destroy();