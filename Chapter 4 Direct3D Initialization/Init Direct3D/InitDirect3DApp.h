#pragma once

#include "../../Common/d3dApp.h"
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
};
