#include "pch.h"
#include "directxapi.h"

#if defined(PV_RENDERING_API_DIRECTX) || defined(PV_RENDERING_API_BOTH)

#include "platform/gethwnd.h"

#define CHECK_AND_POST_ERROR(hr, string, ...) { if (FAILED(hr)) { PV_POST_ERROR(string); __VA_ARGS__; return false; }}

namespace prev {
	GraphicsAPI * GraphicsAPI::UseDirectX(void * windowRawPointer, WindowAPI windowApi, GraphicsDesc & graphicsDesc) {
		DirectXAPI * api = new DirectXAPI(windowRawPointer, windowApi, graphicsDesc);

		if (!api->m_Status) {
			PV_POST_ERROR("Unable to Use DirectX11");
			delete api;
			return nullptr;
		}

		return api;
	}

	DirectXAPI::DirectXAPI(void * windowRawPointer, WindowAPI windowApi, GraphicsDesc & graphicsDesc) {
		m_Data.Vsync		= graphicsDesc.Vsync;
		m_Data.Fullscreen	= graphicsDesc.Fullscreen;
		
		if (windowApi == WindowAPI::WINDOWING_API_WIN32) {
			m_Data.HWnd = (HWND)windowRawPointer;
		} else if (windowApi == WindowAPI::WINDOWING_API_GLFW) {
			m_Data.HWnd = GetHWND(windowRawPointer);
		}

		m_Status = CheckVideoAdapter();
		if (!m_Status) {
			PV_POST_ERROR("Unable to get video card props");
			return;
		}

		m_Status = CreateDeviceAndSwapChain();
		if (!m_Status) {
			PV_POST_ERROR("Unable to Initialize DirectX 11");
			return;
		}

		m_RenderingAPI = RenderingAPI::RENDERING_API_DIRECTX;

		return;
	}

	DirectXAPI::~DirectXAPI() {
	}

	void DirectXAPI::StartFrame() {
		static float color[] = { 0, 0, 1, 1 };
		m_Data.DeviceContext->ClearRenderTargetView(m_Data.RenderTarget.Get(), color);
		m_Data.DeviceContext->ClearDepthStencilView(m_Data.DepthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
		return;
	}

	void DirectXAPI::EndFrame() {
		if (m_Data.Vsync)
			m_Data.SwapChain->Present(1, 0);
		else
			m_Data.SwapChain->Present(0, 0);
		return;
	}

	void DirectXAPI::OnEvent(Event & e) {
	}

	void DirectXAPI::SetFullscreen(bool fullscreen) {
		m_Data.SwapChain->SetFullscreenState(fullscreen, NULL);
		m_Data.Fullscreen = fullscreen;
		ChangeWindowResolution(m_Data.CurrentModeDescriptionIndex);
	}

	void DirectXAPI::ChangeResolution(int index) {
		ChangeWindowResolution(index);
	}

	std::vector<std::pair<unsigned int, unsigned int>> DirectXAPI::GetSupportedResolution() {
		std::vector<std::pair<unsigned int, unsigned int>> supportedResolution;
		for (unsigned int i = 0; i < m_Data.AllDisplayModes.size(); i++) {
			supportedResolution.push_back(std::make_pair(m_Data.AllDisplayModes[i].Width, m_Data.AllDisplayModes[i].Height));
		}
		return supportedResolution;
	}

	bool DirectXAPI::ChangeWindowResolution(int index) {

		index = index % m_Data.AllDisplayModes.size();

		HRESULT hr;

		m_Data.CurrentModeDescriptionIndex = index % m_Data.AllDisplayModes.size();

		DXGI_MODE_DESC zeroRefrreshRate = m_Data.AllDisplayModes[m_Data.CurrentModeDescriptionIndex];
		zeroRefrreshRate.RefreshRate.Numerator = 0;
		zeroRefrreshRate.RefreshRate.Denominator = 0;

		BOOL isFullscreen = false;
		m_Data.SwapChain->GetFullscreenState(&isFullscreen, NULL);

		if (m_Data.Fullscreen != isFullscreen) {
			if (isFullscreen) {
				hr = m_Data.SwapChain->ResizeTarget(&zeroRefrreshRate);
				CHECK_AND_POST_ERROR(hr, "Unable to resize target!");

				hr = m_Data.SwapChain->SetFullscreenState(true, NULL);
				CHECK_AND_POST_ERROR(hr, "Unable to switch to fullscreen mode!");
			} else {
				hr = m_Data.SwapChain->SetFullscreenState(true, NULL);
				CHECK_AND_POST_ERROR(hr, "Unable to switch to windowed mode mode!");

				RECT rect = { 0, 0, m_Data.AllDisplayModes[m_Data.CurrentModeDescriptionIndex].Width, 
					m_Data.AllDisplayModes[m_Data.CurrentModeDescriptionIndex].Height };
				BOOL result = AdjustWindowRectEx(&rect, WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU, NULL, WS_EX_APPWINDOW);
				CHECK_AND_POST_ERROR(result, "Unable to adjust window rectangle!");

				SetWindowPos(m_Data.HWnd, HWND_TOP, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE);

			}

			m_Data.Fullscreen = !m_Data.Fullscreen;

		}

		hr = m_Data.SwapChain->ResizeTarget(&zeroRefrreshRate);
		CHECK_AND_POST_ERROR(hr, "Unable to resize target!");

		m_Data.DeviceContext->ClearState();
		m_Data.RenderTarget			= nullptr;
		m_Data.DepthStencilBuffer	= nullptr;
		m_Data.DepthStencilState	= nullptr;
		m_Data.DepthStencilView		= nullptr;
		m_Data.RasterizerState		= nullptr;

		hr = m_Data.SwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
		CHECK_AND_POST_ERROR(hr, "Direct3D was unable to resize the swap chain!");

		Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;

		hr = m_Data.SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)backBuffer.GetAddressOf());
		CHECK_AND_POST_ERROR(hr, "Unable to get back buffer");

		hr = m_Data.Device->CreateRenderTargetView(backBuffer.Get(), NULL, m_Data.RenderTarget.GetAddressOf());
		CHECK_AND_POST_ERROR(hr, "Unable to create render target view");

		hr = CreateDepthStencilBuffer(m_Data.AllDisplayModes[m_Data.CurrentModeDescriptionIndex].Width,
									  m_Data.AllDisplayModes[m_Data.CurrentModeDescriptionIndex].Height);
		CHECK_AND_POST_ERROR(hr, "Unable to create render depth stencil buffer");

		hr = CreateDepthStencilState();
		CHECK_AND_POST_ERROR(hr, "Unable to create depth stencil state");

		hr = CreateDepthStencilView();
		CHECK_AND_POST_ERROR(hr, "Unable to create depth stencil view");

		m_Data.DeviceContext->OMSetRenderTargets(1, m_Data.RenderTarget.GetAddressOf(), m_Data.DepthStencilView.Get());

		hr = CreateRasterizerState();
		CHECK_AND_POST_ERROR(hr, "Unable to create rasterizer state");

		m_Data.DeviceContext->RSSetState(m_Data.RasterizerState.Get());

		D3D11_VIEWPORT viewport = CreateViewport(m_Data.AllDisplayModes[m_Data.CurrentModeDescriptionIndex].Width,
												 m_Data.AllDisplayModes[m_Data.CurrentModeDescriptionIndex].Height);

		m_Data.DeviceContext->RSSetViewports(1, &viewport);

		return false;
	}

	bool DirectXAPI::CheckVideoAdapter() {
		Microsoft::WRL::ComPtr<IDXGIFactory> d3dFactory;
		Microsoft::WRL::ComPtr<IDXGIAdapter> videoAdapter;
		Microsoft::WRL::ComPtr<IDXGIOutput> videoOutput;

		UINT numModes = 0;
		DXGI_MODE_DESC * outputModes = nullptr;
		DXGI_ADAPTER_DESC adapterDesc;

		HRESULT hr;
		hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void **)d3dFactory.GetAddressOf());
		CHECK_AND_POST_ERROR(hr, "Unable to get DXGIFactory");

		hr = d3dFactory->EnumAdapters(0, videoAdapter.GetAddressOf());
		CHECK_AND_POST_ERROR(hr, "Unable to get primary adapter");

		hr = videoAdapter->GetDesc(&adapterDesc);
		CHECK_AND_POST_ERROR(hr, "Unable to get Video adapter description");

		hr = videoAdapter->EnumOutputs(0, videoOutput.GetAddressOf());
		CHECK_AND_POST_ERROR(hr, "Unable to get primary adapter output");

		hr = videoOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, 0, &numModes, NULL);
		CHECK_AND_POST_ERROR(hr, "Unable to get primary output display mode list");

		outputModes = new DXGI_MODE_DESC[numModes];

		hr = videoOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, 0, &numModes, outputModes);
		CHECK_AND_POST_ERROR(hr, "Unable to get primary output display mode list", delete[] outputModes);

		for (UINT i = 0; i < numModes; i++) {
			if (i == 0) { m_Data.AllDisplayModes.push_back(outputModes[i]); continue; }
			if (m_Data.AllDisplayModes[m_Data.AllDisplayModes.size() - 1].Width == outputModes[i].Width
				&& m_Data.AllDisplayModes[m_Data.AllDisplayModes.size() - 1].Height == outputModes[i].Height) 
				continue;
			else
				m_Data.AllDisplayModes.push_back(outputModes[i]);
		}

		m_Data.CurrentModeDescriptionIndex = m_Data.AllDisplayModes.size() - 1;

		delete[] outputModes;

		m_Data.AdapterDesc = std::string(_bstr_t(adapterDesc.Description));
		m_Data.DedicatedVideoMemory = (UINT)adapterDesc.DedicatedVideoMemory / (1024 * 1024);

		return true;
	}

	bool DirectXAPI::CreateDeviceAndSwapChain() {

		UINT width = m_Data.AllDisplayModes[m_Data.CurrentModeDescriptionIndex].Width;
		UINT height = m_Data.AllDisplayModes[m_Data.CurrentModeDescriptionIndex].Height;

		DXGI_SWAP_CHAIN_DESC swapChainDesc;
		ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));

		swapChainDesc.BufferCount = 2;
		swapChainDesc.BufferDesc.Width = width;
		swapChainDesc.BufferDesc.Height = height;
		swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

		if (m_Data.Vsync) {
			swapChainDesc.BufferDesc.RefreshRate.Numerator = m_Data.RefreshRate.Numerator;
			swapChainDesc.BufferDesc.RefreshRate.Denominator = m_Data.RefreshRate.Denominator;
		} else {
			swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
			swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
		}

		swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		swapChainDesc.OutputWindow = m_Data.HWnd;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

		if (m_Data.Fullscreen)
			swapChainDesc.Windowed = FALSE;
		else
			swapChainDesc.Windowed = TRUE;

		D3D_FEATURE_LEVEL featureLevels[] = {
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
		};

		D3D_FEATURE_LEVEL selectedFeatureLevel;

		HRESULT hr;

		hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
										   D3D11_CREATE_DEVICE_DEBUG, featureLevels, (UINT)std::size(featureLevels), D3D11_SDK_VERSION,
										   &swapChainDesc, m_Data.SwapChain.GetAddressOf(), m_Data.Device.GetAddressOf(), &selectedFeatureLevel,
										   m_Data.DeviceContext.GetAddressOf());
		CHECK_AND_POST_ERROR(hr, "Unable to create D3D Device and swap chain");

		switch (selectedFeatureLevel) {
		case D3D_FEATURE_LEVEL_11_1:
			m_Data.DirectXFeatureLevel = "D3D_FEATURE_LEVEL_11_1";
			break;
		case D3D_FEATURE_LEVEL_11_0:
			m_Data.DirectXFeatureLevel = "D3D_FEATURE_LEVEL_11_0";
			break;
		case D3D_FEATURE_LEVEL_10_1:
			m_Data.DirectXFeatureLevel = "D3D_FEATURE_LEVEL_10_1";
			break;
		case D3D_FEATURE_LEVEL_10_0:
			m_Data.DirectXFeatureLevel = "D3D_FEATURE_LEVEL_10_0";
			break;
		case D3D_FEATURE_LEVEL_9_3:
			m_Data.DirectXFeatureLevel = "D3D_FEATURE_LEVEL_9_3";
			break;
		case D3D_FEATURE_LEVEL_9_2:
			m_Data.DirectXFeatureLevel = "D3D_FEATURE_LEVEL_9_2";
			break;
		case D3D_FEATURE_LEVEL_9_1:
			m_Data.DirectXFeatureLevel = "D3D_FEATURE_LEVEL_9_1";
			break;
		default:
			PV_POST_ERROR("DirectX 11 Not supported");
			return false;
			break;
		}

		m_Data.FeatureLevel = selectedFeatureLevel;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;

		hr = m_Data.SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)backBuffer.GetAddressOf());
		CHECK_AND_POST_ERROR(hr, "Unable to get back buffer");

		hr = m_Data.Device->CreateRenderTargetView(backBuffer.Get(), NULL, m_Data.RenderTarget.GetAddressOf());
		CHECK_AND_POST_ERROR(hr, "Unable to create render target view");

		hr = CreateDepthStencilBuffer(width, height);
		CHECK_AND_POST_ERROR(hr, "Unable to create render depth stencil buffer");

		hr = CreateDepthStencilState();
		CHECK_AND_POST_ERROR(hr, "Unable to create depth stencil state");

		hr = CreateDepthStencilView();
		CHECK_AND_POST_ERROR(hr, "Unable to create depth stencil view");

		m_Data.DeviceContext->OMSetRenderTargets(1, m_Data.RenderTarget.GetAddressOf(), m_Data.DepthStencilView.Get());

		hr = CreateRasterizerState();
		CHECK_AND_POST_ERROR(hr, "Unable to create rasterizer state");

		m_Data.DeviceContext->RSSetState(m_Data.RasterizerState.Get());

		D3D11_VIEWPORT viewport = CreateViewport(width, height);

		m_Data.DeviceContext->RSSetViewports(1, &viewport);

		return true;
	}

	HRESULT DirectXAPI::CreateDepthStencilBuffer(UINT width, UINT height) {
		D3D11_TEXTURE2D_DESC depthBufferDesc;
		ZeroMemory(&depthBufferDesc, sizeof(depthBufferDesc));

		depthBufferDesc.ArraySize			= 1;
		depthBufferDesc.BindFlags			= D3D11_BIND_DEPTH_STENCIL;
		depthBufferDesc.CPUAccessFlags		= 0;
		depthBufferDesc.Format				= DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthBufferDesc.Width				= width;
		depthBufferDesc.Height				= height;
		depthBufferDesc.MipLevels			= 1;
		depthBufferDesc.MiscFlags			= 0;
		depthBufferDesc.SampleDesc.Count	= 1;
		depthBufferDesc.SampleDesc.Quality	= 0;
		depthBufferDesc.Usage				= D3D11_USAGE_DEFAULT;

		return m_Data.Device->CreateTexture2D(&depthBufferDesc, NULL, m_Data.DepthStencilBuffer.GetAddressOf());
	}

	HRESULT DirectXAPI::CreateDepthStencilState() {
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
		ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));

		depthStencilDesc.DepthEnable					= true;
		depthStencilDesc.DepthWriteMask					= D3D11_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc						= D3D11_COMPARISON_LESS;
		depthStencilDesc.StencilEnable					= true;
		depthStencilDesc.StencilReadMask				= 0xFF;
		depthStencilDesc.StencilWriteMask				= 0xFF;
		depthStencilDesc.FrontFace.StencilFailOp		= D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilDepthFailOp	= D3D11_STENCIL_OP_INCR;
		depthStencilDesc.FrontFace.StencilPassOp		= D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilFunc			= D3D11_COMPARISON_ALWAYS;
		depthStencilDesc.BackFace.StencilFailOp			= D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilDepthFailOp	= D3D11_STENCIL_OP_DECR;
		depthStencilDesc.BackFace.StencilPassOp			= D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilFunc			= D3D11_COMPARISON_ALWAYS;

		return m_Data.Device->CreateDepthStencilState(&depthStencilDesc, m_Data.DepthStencilState.GetAddressOf());
	}

	HRESULT DirectXAPI::CreateDepthStencilView() {
		D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
		ZeroMemory(&depthStencilViewDesc, sizeof(depthStencilViewDesc));

		depthStencilViewDesc.Format						= DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthStencilViewDesc.ViewDimension				= D3D11_DSV_DIMENSION_TEXTURE2D;
		depthStencilViewDesc.Texture2D.MipSlice			= 0;

		return m_Data.Device->CreateDepthStencilView(m_Data.DepthStencilBuffer.Get(), &depthStencilViewDesc, m_Data.DepthStencilView.GetAddressOf());
	}

	HRESULT DirectXAPI::CreateRasterizerState() {
		D3D11_RASTERIZER_DESC rasterDesc;
		ZeroMemory(&rasterDesc, sizeof(rasterDesc));

		rasterDesc.AntialiasedLineEnable	= false;
		rasterDesc.CullMode					= D3D11_CULL_BACK;
		rasterDesc.DepthBias				= 0;
		rasterDesc.DepthBiasClamp			= 0.0f;
		rasterDesc.DepthClipEnable			= true;
		rasterDesc.FillMode					= D3D11_FILL_SOLID;
		rasterDesc.FrontCounterClockwise	= false;
		rasterDesc.MultisampleEnable		= false;
		rasterDesc.ScissorEnable			= false;
		rasterDesc.SlopeScaledDepthBias		= 0.0f;

		return m_Data.Device->CreateRasterizerState(&rasterDesc, m_Data.RasterizerState.GetAddressOf());
	}

	D3D11_VIEWPORT DirectXAPI::CreateViewport(UINT width, UINT height) {
		D3D11_VIEWPORT viewport;
		viewport.Width		= (float)width;
		viewport.Height		= (float)height;
		viewport.MinDepth	= 0.0f;
		viewport.MaxDepth	= 1.0f;
		viewport.TopLeftX	= 0.0f;
		viewport.TopLeftY	= 0.0f;
		return viewport;
	}

}

#endif