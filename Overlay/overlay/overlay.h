#pragma once

#include <d3d11.h>
#include <string>

#include "../imgui/imgui.h"

class Overlay
{
private:
	HWND m_hwnd = nullptr;
	HWND m_targetHwnd = nullptr;
	HINSTANCE m_instance = nullptr;
	std::wstring m_windowName;
	std::wstring m_className;
	bool m_overlayRun = true;
	bool m_showGui = true;

public:
	ImVec2 size{ };
	ImVec2 position{ };

	void findAttachWindow(const std::string& windowName, const std::string& className);
	bool createWindow(const std::wstring& windowName, const std::wstring& className);
	bool initializeImGui();
	void updateWindow();
	bool isWindowFocus();
	void cleanup();
	void mainLoop();
};

class D3D
{
public:
	ID3D11Device* D3DDevice = nullptr;
	ID3D11DeviceContext* D3DDeviceContext = nullptr;
	IDXGISwapChain* swapChain = nullptr;
	ID3D11RenderTargetView* renderTargetView = nullptr;

	bool initializeD3D(const HWND& hwnd);
	bool createRenderTarget();
	void cleanupRenderTarget();
	void cleanupD3D11();
};
inline D3D d3d;