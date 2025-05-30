#include "overlay.h"

#include "../imgui/imgui_impl_win32.h"
#include "../imgui/imgui_impl_dx11.h"

#include <stdexcept>
#include <dwmapi.h>
#include <iostream>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam))
		return true;

	switch (message)
	{
	case WM_SIZE:
		if (d3d.D3DDevice != nullptr && wParam != SIZE_MINIMIZED)
		{
			d3d.cleanupRenderTarget();
			d3d.swapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
			d3d.createRenderTarget();
		}
		return false;

	case WM_DESTROY:
		PostQuitMessage(0);
		return false;
	}

	return DefWindowProc(window, message, wParam, lParam);
}

void Overlay::findAttachWindow(const std::string& targetWindowName, const std::string& targetClassName)
{
	m_targetHwnd = FindWindowA(targetClassName.c_str(), targetWindowName.c_str());

	if (!m_targetHwnd)
	{
		std::cout << "Failed to find the target window.";
		Sleep(2500);
		exit(1);
	}

	if (!createWindow(L"Overlay", L"Overlay"))
	{
		std::cout << "Failed to create the window or initialize D3D11.";
		Sleep(2500);
		exit(1);
	}

	if (!initializeImGui())
	{
		std::cout << "Failed to initialize ImGui.";
		Sleep(2500);
		exit(1);
	}

	SetForegroundWindow(m_targetHwnd);

	mainLoop();
}

bool Overlay::createWindow(const std::wstring& windowName, const std::wstring& className)
{
	WNDCLASSEXW wc{ };
	ZeroMemory(&wc, sizeof(wc));
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.style = CS_CLASSDC;
	wc.lpfnWndProc = windowProc;
	wc.cbClsExtra = NULL;
	wc.cbWndExtra = NULL;
	wc.hInstance = GetModuleHandle(NULL);
	wc.hIcon = nullptr;
	wc.hCursor = nullptr;
	wc.hbrBackground = nullptr;
	wc.lpszMenuName = nullptr;
	wc.lpszClassName = className.c_str();
	wc.hIconSm = nullptr;

	m_instance = wc.hInstance;
	m_windowName = windowName;
	m_className = className;

	if (!RegisterClassExW(&wc))
		return false;

	m_hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW, className.c_str(), windowName.c_str(), WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT, 100, 100, NULL, NULL, GetModuleHandle(NULL), NULL);

	if (!IsWindow(m_hwnd))
		return false;

	const MARGINS margins{ -1, -1, -1, -1 };
	DwmExtendFrameIntoClientArea(m_hwnd, &margins);

	if (!d3d.initializeD3D(m_hwnd))
	{
		d3d.cleanupD3D11();
		UnregisterClassW(wc.lpszClassName, wc.hInstance);
		return false;
	}

	ShowWindow(m_hwnd, SW_SHOWDEFAULT);
	UpdateWindow(m_hwnd);

	return true;
}

bool Overlay::initializeImGui()
{
	ImGui::CreateContext();

	if (!ImGui_ImplWin32_Init(m_hwnd) || !ImGui_ImplDX11_Init(d3d.D3DDevice, d3d.D3DDeviceContext))
		return false;

	return true;
}

void Overlay::updateWindow()
{
	if (!IsWindow(m_hwnd) || !IsWindow(m_targetHwnd))
		m_overlayRun = false;

	RECT rect{ };
	GetClientRect(m_targetHwnd, &rect);
	size = ImVec2(static_cast<float>(rect.right - rect.left), static_cast<float>(rect.bottom - rect.top));

	POINT point{ };
	ClientToScreen(m_targetHwnd, &point);
	position = ImVec2(static_cast<float>(point.x), static_cast<float>(point.y));

	SetWindowPos(m_hwnd, HWND_TOP, position.x, position.y, size.x, size.y, SWP_SHOWWINDOW);

	POINT MousePos;
	GetCursorPos(&MousePos);
	ScreenToClient(m_hwnd, &MousePos);

	ImGui::GetIO().MousePos.x = static_cast<float>(MousePos.x);
	ImGui::GetIO().MousePos.y = static_cast<float>(MousePos.y);

	if (ImGui::GetIO().WantCaptureMouse)
		SetWindowLong(m_hwnd, GWL_EXSTYLE, GetWindowLong(m_hwnd, GWL_EXSTYLE) & (~WS_EX_LAYERED));
	else
		SetWindowLong(m_hwnd, GWL_EXSTYLE, GetWindowLong(m_hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
}

bool Overlay::isWindowFocus()
{
	HWND hForeground = GetForegroundWindow();

	if (hForeground != m_targetHwnd && hForeground != m_hwnd)
		return false;

	return true;
}

void Overlay::cleanup()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	d3d.cleanupD3D11();
	if (IsWindow(m_hwnd))
		DestroyWindow(m_hwnd);
	UnregisterClassW(m_className.c_str(), m_instance);
}

void Overlay::mainLoop()
{
	while (m_overlayRun)
	{
		MSG msg;
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
				m_overlayRun = false;
		}

		updateWindow();

		if (!isWindowFocus())
		{
			const float color[4] = { 0, 0, 0, 0 };
			d3d.D3DDeviceContext->OMSetRenderTargets(1, &d3d.renderTargetView, NULL);
			d3d.D3DDeviceContext->ClearRenderTargetView(d3d.renderTargetView, color);

			d3d.swapChain->Present(0, 0);
			continue;
		}

		if (GetAsyncKeyState(VK_END) & 1)
			m_overlayRun = false;

		if (GetAsyncKeyState(VK_INSERT) & 1)
			m_showGui = !m_showGui;

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		if (m_showGui)
		{
			ImGui::SetNextWindowSize(ImVec2{ 550,350 });
			ImGui::Begin("Overlay");
			ImGui::Text("Hello!");
			ImGui::End();
		}

		ImGui::Render();

		const float color[4] = { 0, 0, 0, 0 };
		d3d.D3DDeviceContext->OMSetRenderTargets(1, &d3d.renderTargetView, NULL);
		d3d.D3DDeviceContext->ClearRenderTargetView(d3d.renderTargetView, color);

		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		d3d.swapChain->Present(0, 0);
	}

	cleanup();
}

bool D3D::initializeD3D(const HWND& hwnd)
{
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 2;
	sd.BufferDesc.Width = 0;
	sd.BufferDesc.Height = 0;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 144;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hwnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };

	HRESULT result = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, NULL, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &swapChain, &D3DDevice, &featureLevel, &D3DDeviceContext);

	if (result == DXGI_ERROR_UNSUPPORTED)
		result = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_WARP, NULL, NULL, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &swapChain, &D3DDevice, &featureLevel, &D3DDeviceContext);
	if (result != S_OK)
		return false;

	if (!createRenderTarget())
		return false;

	return true;
}

bool D3D::createRenderTarget()
{
	ID3D11Texture2D* backBuffer = nullptr;

	swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));

	if (!backBuffer)
		return false;

	D3DDevice->CreateRenderTargetView(backBuffer, NULL, &renderTargetView);
	backBuffer->Release();

	return true;
}

void D3D::cleanupRenderTarget()
{
	if (renderTargetView)
	{
		renderTargetView->Release();
		renderTargetView = NULL;
	}
}

void D3D::cleanupD3D11()
{
	cleanupRenderTarget();
	if (swapChain)
	{
		swapChain->Release(); swapChain = NULL;
	}

	if (D3DDeviceContext)
	{
		D3DDeviceContext->Release(); D3DDeviceContext = NULL;
	}

	if(D3DDevice)
	{
		D3DDevice->Release(); D3DDevice = NULL;
	}
}