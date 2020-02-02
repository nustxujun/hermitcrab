#include "ImGuiOverlay.h"
#include "Framework.h"

ImGuiPass::ImGuiPass()
{
	setName("ImGui");
	initImGui();
	initRendering();
	initFonts();

	auto p = std::bind(&ImGuiPass::process, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
	Framework::setProcessor(p);
}

ImGuiPass::~ImGuiPass()
{
	ImGuiOverlay::ImGuiObject::clear();
	Framework::setProcessor({});
	ImGui::DestroyContext();
}


void ImGuiPass::initImGui()
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

void ImGuiPass::initRendering()
{
	auto renderer = Renderer::getSingleton();

	auto vs = renderer->compileShaderFromFile("shaders/imgui.hlsl", "vs", SM_VS);
	auto ps = renderer->compileShaderFromFile("shaders/imgui.hlsl", "ps", SM_PS);
	std::vector<Renderer::Shader::Ptr> shaders = { vs, ps };
	vs->enable32BitsConstants(true);
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

	mPipelineState = renderer->createPipelineState(shaders, rs);
	mConstant = mPipelineState->createConstantBuffer(Renderer::Shader::ST_VERTEX,"vertexBuffer");

}


void ImGuiPass::draw(ImDrawData* data)
{
	if (data->DisplaySize.x <= 0.0f || data->DisplaySize.y <= 0.0f || data->TotalIdxCount <= 0 )
		return;

	auto renderer = Renderer::getSingleton();
	auto& VertexBuffer = mVertexBuffer[renderer->getCurrentFrameIndex()];
	auto& IndexBuffer = mIndexBuffer[renderer->getCurrentFrameIndex()];
	if (!VertexBuffer || VertexBuffer->getSize() < data->TotalVtxCount * sizeof(ImDrawVert))
	{
		auto size = data->TotalIdxCount;
		VertexBuffer = renderer->createBuffer(size * sizeof(ImDrawVert), sizeof(ImDrawVert), D3D12_HEAP_TYPE_UPLOAD);
	}

	if (!IndexBuffer || IndexBuffer->getSize() < data->TotalIdxCount * sizeof(ImDrawIdx))
	{
		auto size = data->TotalIdxCount ;
		IndexBuffer = renderer->createBuffer(size * sizeof(ImDrawIdx), sizeof(ImDrawIdx), D3D12_HEAP_TYPE_UPLOAD);
	}

	auto vertices = VertexBuffer->getResource()->map(0);
	auto indices = IndexBuffer->getResource()->map(0);

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

	VertexBuffer->getResource()->unmap(0);
	IndexBuffer->getResource()->unmap(0);

	float L = data->DisplayPos.x;
	float R = data->DisplayPos.x + data->DisplaySize.x;
	float T = data->DisplayPos.y;
	float B = data->DisplayPos.y + data->DisplaySize.y;

	float mvp[4][4] =
	{
		{ 2.0f / (R - L),   0.0f,           0.0f,      (R + L) / (L - R) },
		{ 0.0f,         2.0f / (T - B),     0.0f,      (T + B) / (B - T) },
		{ 0.0f,         0.0f,           0.5f,       0.5f },
		{ 0.0f,  0.0f,   0.0f,       1.0f },
	};

	auto cmdlist = renderer->getCommandList();
	cmdlist->setPipelineState(mPipelineState);

	//mConstant->blit(mvp);
	//mPipelineState->setVSConstant("vertexBuffer", mConstant);
	mPipelineState->setVSVariable("ProjectionMatrix", mvp);

	//cmdlist->set32BitConstants(1,16,mvp,0);

	D3D12_VIEWPORT vp = {0};
	vp.Width = data->DisplaySize.x;
	vp.Height = data->DisplaySize.y;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = vp.TopLeftX = 0;
	cmdlist->setViewport(vp);
	cmdlist->setVertexBuffer(VertexBuffer);
	cmdlist->setIndexBuffer(IndexBuffer);
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
				mPipelineState->setPSResource("texture0", *handle);
				//cmdlist->get()->SetGraphicsRootDescriptorTable(0, *handle);
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

LRESULT ImGuiPass::process(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	auto& io = ImGui::GetIO();
	switch (message)
	{
	case WM_LBUTTONDOWN: io.MouseDown[0] = true;break;
	case WM_LBUTTONUP: io.MouseDown[0] = false; break;
	case WM_RBUTTONDOWN: io.MouseDown[1] = true; break;
	case WM_RBUTTONUP: io.MouseDown[1] = false; break;
	case WM_MBUTTONDOWN: io.MouseDown[2] = true; break;
	case WM_MBUTTONUP: io.MouseDown[2] = false; break;

	case WM_MOUSEMOVE: {io.MousePos.x = (float)GET_X_LPARAM(lParam); io.MousePos.y = (float)GET_Y_LPARAM(lParam);} break;
	}

	return DefWindowProcW(hWnd, message, wParam, lParam);
}

void ImGuiPass::setup()
{
	HWND win = Renderer::getSingleton()->getWindow();
	RECT rect;
	::GetClientRect(win,&rect);
	resize(win, std::max(1L, rect.right), std::max(1L,rect.bottom));
}

void ImGuiPass::compile(const RenderGraph::Inputs & inputs)
{
	write(inputs[0]->getRenderTarget());
}

void ImGuiPass::execute()
{
	
	ImGui::NewFrame();
	//ImGui::ShowDemoWindow();
	ImGuiOverlay::ImGuiObject::root()->framemove();

	ImGui::Render();

	auto data = ImGui::GetDrawData();
	draw(data);
}

void ImGuiPass::resize(HWND win, int width, int height)
{
	if (width == mWidth && height == mHeight)
		return ;

	auto& io = ImGui::GetIO();
	io.DisplaySize = { (float)width, (float)height };
	io.ImeWindowHandle = win;

}


void ImGuiPass::initFonts()
{
	// Build texture atlas
	ImGuiIO& io = ImGui::GetIO();
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	auto renderer = Renderer::getSingleton();
	mFonts = renderer->createTexture(width, height, DXGI_FORMAT_R8G8B8A8_UNORM,1, pixels);

	static_assert(sizeof(ImTextureID) >= sizeof(mFonts->getShaderResource().ptr), "Can't pack descriptor handle into TexID, 32-bit not supported yet.");
	io.Fonts->TexID = (ImTextureID)mFonts->getShaderResource().ptr;
}
