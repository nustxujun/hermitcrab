#include "ImguiOverlay.h"
#include "Framework.h"

ImguiPass::ImguiPass()
{
	initImGui();
	initRendering();
	initFonts();

	auto p = std::bind(&ImguiPass::process, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
	Framework::setProcessor(p);
}

ImguiPass::~ImguiPass()
{
	Framework::setProcessor({});
	ImGui::DestroyContext();
}


void ImguiPass::initImGui()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsClassic();


	//mInput->listen([&io](const Input::Mouse& mouse, const Input::Keyboard& keyboard) {
	//	io.MousePos = { (float)mouse.x, (float)mouse.y };
	//	io.MouseDown[0] = mouse.leftButton;
	//	io.MouseDown[1] = mouse.rightButton;
	//	io.MouseDown[2] = mouse.middleButton;
	//	//io.MouseWheel = mouse.scrollWheelValue;
	//	
	//	return io.WantCaptureMouse;
	//	}, 2);

}

void ImguiPass::initRendering()
{
	auto renderer = Renderer::getSingleton();

	auto vs = renderer->compileShader(L"shaders/imgui.hlsl", L"vs", L"vs_5_0");
	auto ps = renderer->compileShader(L"shaders/imgui.hlsl", L"ps", L"ps_5_0");
	std::vector<Renderer::Shader::Ptr> shaders = { vs, ps };
	ps->registerStaticSampler({
		D3D12_FILTER_MIN_MAG_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		0,0,
		D3D12_COMPARISON_FUNC_NEVER,
		D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
		0,
		D3D12_FLOAT32_MAX,
		0,0,
		D3D12_SHADER_VISIBILITY_PIXEL
		});
	
	std::vector<Renderer::RootParameter> rootparams = { D3D12_SHADER_VISIBILITY_PIXEL ,D3D12_SHADER_VISIBILITY_VERTEX };
	rootparams[0].srv(0, 0);
	rootparams[1].cbv32(0,0,16);


	Renderer::RenderState rs = Renderer::RenderState::Default;
	rs.setInputLayout({
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (size_t)(&((ImDrawVert*)0)->pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (size_t)(&((ImDrawVert*)0)->uv),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, (size_t)(&((ImDrawVert*)0)->col), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		});
	{
		D3D12_BLEND_DESC desc = {};
		desc.AlphaToCoverageEnable = false;
		desc.RenderTarget[0].BlendEnable = true;
		desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		rs.setBlend(desc);
	}
	{
		D3D12_RASTERIZER_DESC desc = {};
		desc.FillMode = D3D12_FILL_MODE_SOLID;
		desc.CullMode = D3D12_CULL_MODE_NONE;
		desc.FrontCounterClockwise = FALSE;
		desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		desc.DepthClipEnable = true;
		desc.MultisampleEnable = FALSE;
		desc.AntialiasedLineEnable = FALSE;
		desc.ForcedSampleCount = 0;
		desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		rs.setRasterizer(desc);
	}

	{
		D3D12_DEPTH_STENCIL_DESC desc = {};
		desc.DepthEnable = false;
		desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		desc.StencilEnable = false;
		desc.FrontFace.StencilFailOp = desc.FrontFace.StencilDepthFailOp = desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
		desc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		desc.BackFace = desc.FrontFace;

		rs.setDepthStencil(desc);
	}

	mPipelineState = renderer->createPipelineState(shaders, rs, rootparams);
}


void ImguiPass::draw(ImDrawData* data)
{
	if (data->DisplaySize.x <= 0.0f || data->DisplaySize.y <= 0.0f || data->TotalIdxCount <= 0)
		return;

	auto renderer = Renderer::getSingleton();
	if (!mVertexBuffer || mVertexBuffer->getSize() < data->TotalVtxCount * sizeof(ImDrawVert))
	{
		auto size = data->TotalIdxCount * 2;
		mVertexBuffer = renderer->createBuffer(size * sizeof(ImDrawVert), sizeof(ImDrawVert), D3D12_HEAP_TYPE_UPLOAD);
	}

	if (!mIndexBuffer || mIndexBuffer->getSize() < data->TotalIdxCount * sizeof(ImDrawIdx))
	{
		auto size = data->TotalIdxCount * 2;
		mIndexBuffer = renderer->createBuffer(size * sizeof(ImDrawIdx), sizeof(ImDrawIdx), D3D12_HEAP_TYPE_UPLOAD);
	}

	auto vertices = mVertexBuffer->getResource()->map(0);
	auto indices = mIndexBuffer->getResource()->map(0);

	for (int n = 0; n < data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = data->CmdLists[n];
		auto numVertices = cmd_list->VtxBuffer.Size * sizeof(ImDrawVert);
		auto numIndices = cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx);
		memcpy(vertices, cmd_list->VtxBuffer.Data, numVertices);
		vertices += numVertices;
		memcpy(indices, cmd_list->IdxBuffer.Data,numIndices);
		indices += numIndices;
	}

	mVertexBuffer->getResource()->unmap(0);
	mIndexBuffer->getResource()->unmap(0);

	float L = data->DisplayPos.x;
	float R = data->DisplayPos.x + data->DisplaySize.x;
	float T = data->DisplayPos.y;
	float B = data->DisplayPos.y + data->DisplaySize.y;
	//float mvp[4][4] =
	//{
	//	{ 2.0f / (R - L),   0.0f,           0.0f,       0.0f },
	//	{ 0.0f,         2.0f / (T - B),     0.0f,       0.0f },
	//	{ 0.0f,         0.0f,           0.5f,       0.0f },
	//	{ (R + L) / (L - R),  (T + B) / (B - T),    0.5f,       1.0f },
	//};

	float mvp[4][4] =
	{
		{ 2.0f / (R - L),   0.0f,           0.0f,      (R + L) / (L - R) },
		{ 0.0f,         2.0f / (T - B),     0.0f,      (T + B) / (B - T) },
		{ 0.0f,         0.0f,           0.5f,       0.5f },
		{ 0.0f,  0.0f,   0.0f,       1.0f },
	};

	auto cmdlist = renderer->getCommandList();
	cmdlist->setPipelineState(mPipelineState);
	cmdlist->set32BitConstants(1,16,mvp,0);

	D3D12_VIEWPORT vp = {0};
	vp.Width = data->DisplaySize.x;
	vp.Height = data->DisplaySize.y;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = vp.TopLeftX = 0;
	cmdlist->setViewport(vp);
	cmdlist->setVertexBuffer(mVertexBuffer);
	cmdlist->setIndexBuffer(mIndexBuffer);
	cmdlist->setPrimitiveType();

	int global_idx_offset = 0;
	int global_vtx_offset = 0;
	ImVec2 clip_off = data->DisplayPos;
	for (int n = 0; n < data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = data->CmdLists[n];
		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			if (pcmd->UserCallback != NULL)
			{

			}
			else
			{
				const D3D12_RECT r = { (LONG)(pcmd->ClipRect.x - clip_off.x), (LONG)(pcmd->ClipRect.y - clip_off.y), (LONG)(pcmd->ClipRect.z - clip_off.x), (LONG)(pcmd->ClipRect.w - clip_off.y) };
				cmdlist->setScissorRect(r);
				auto handle = (D3D12_GPU_DESCRIPTOR_HANDLE*)&pcmd->TextureId;
				cmdlist->get()->SetGraphicsRootDescriptorTable(0, *handle);
				//cmdlist->setTexture(0, mFonts);
				cmdlist->drawIndexedInstanced(pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
			}
		}
		global_idx_offset += cmd_list->IdxBuffer.Size;
		global_vtx_offset += cmd_list->VtxBuffer.Size;
	}

}
#define GET_X_LPARAM(lp)	((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp)	((int)(short)HIWORD(lp))

LRESULT ImguiPass::process(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	auto& io = ImGui::GetIO();
	switch (message)
	{
	case WM_LBUTTONDOWN: io.MouseDown[0] = true;break;
	case WM_LBUTTONUP: io.MouseDown[0] = false; break;
	case WM_MOUSEMOVE: {io.MousePos.x = GET_X_LPARAM(lParam); io.MousePos.y = GET_Y_LPARAM(lParam);} break;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

void ImguiPass::setup()
{
	HWND win = Renderer::getSingleton()->getWindow();
	RECT rect;
	::GetClientRect(win,&rect);
	resize(win, rect.right, rect.bottom);
}

void ImguiPass::compile(const RenderGraph::Inputs & inputs)
{
	write(inputs[0]->getRenderTarget());
}

void ImguiPass::execute()
{
	
	ImGui::NewFrame();
	ImGui::ShowDemoWindow();
	ImguiObject::root()->update();

	ImGui::Render();

	auto data = ImGui::GetDrawData();
	draw(data);
}

void ImguiPass::resize(HWND win, int width, int height)
{
	if (width == mWidth && height == mHeight)
		return ;

	auto& io = ImGui::GetIO();
	io.DisplaySize = { (float)width, (float)height };
	io.ImeWindowHandle = win;

}


void ImguiPass::initFonts()
{
	// Build texture atlas
	ImGuiIO& io = ImGui::GetIO();
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	auto renderer = Renderer::getSingleton();
	mFonts = renderer->createTexture(width, height, DXGI_FORMAT_R8G8B8A8_UNORM);
	renderer->updateResource(mFonts, pixels, width * height * 4, [dst = mFonts](auto cmdlist, auto src) {
		cmdlist->copyTexture(dst, 0, { 0,0,0 }, src, 0, nullptr);
	});

	static_assert(sizeof(ImTextureID) >= sizeof(mFonts->getHandle().gpu.ptr), "Can't pack descriptor handle into TexID, 32-bit not supported yet.");
	io.Fonts->TexID = (ImTextureID)mFonts->getHandle().gpu.ptr;
}