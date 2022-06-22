#include "ImguiManager.h"
#include "imgui.h"

ImguiManager::ImguiManager()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows

	ImGui::StyleColorsDark();
}

ImguiManager::~ImguiManager()
{
	ImGui::DestroyContext();
}
