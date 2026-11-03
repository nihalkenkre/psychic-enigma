#pragma once

#include <imgui.h>

void SceneGraph_Create();
void SceneGraph_Render();
void SceneGraph_AddBasicShapeNode(ImVec2 position);
void SceneGraph_ConnectAttributes();
void SceneGraph_DisconnectAttributes();
void SceneGraph_Destroy();