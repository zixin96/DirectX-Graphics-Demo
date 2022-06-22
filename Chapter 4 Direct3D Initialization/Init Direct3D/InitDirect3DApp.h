#pragma once

#include "../../Common/d3dApp.h"
#include "../../common/imgui.h"

/**
 * \brief Demonstrates the sample framework by initializing Direct3D, clearing the screen, and displaying frame stats.
 */
class InitDirect3DApp : public D3DApp
{
public:
	InitDirect3DApp(HINSTANCE hInstance);
	~InitDirect3DApp() override = default;
	bool Initialize() override;
private:
	void OnResize() override;
	void Update(const GameTimer& gt) override;
	void Draw(const GameTimer& gt) override;

	// Imgui State
	bool   show_demo_window    = true;
	bool   show_another_window = false;
	ImVec4 clear_color         = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
};
