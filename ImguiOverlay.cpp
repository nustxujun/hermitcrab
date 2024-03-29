#include "ImGuiOverlay.h"
#include "Framework.h"
#include "Profile.h"



static struct ImGuiInitializer
{
	ImGuiInitializer()
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsClassic();
	}
	~ImGuiInitializer()
	{
		ImGui::DestroyContext();
	}
} imgui_initializer;


ImGuiPass::ImGuiPass()
{


	//initImGui();
	initRendering();
	initFonts();

	beginFrame();
}

ImGuiPass::~ImGuiPass()
{
	auto r = Renderer::getSingleton();
	if (r)
	{
		for (auto b : mVertexBuffer)
			r->destroyResource(b);

		for (auto b : mIndexBuffer)
			r->destroyResource(b);

		r->destroyResource(mFonts);
	}

}


void ImGuiPass::initImGui()
{

}

void ImGuiPass::initRendering()
{
	auto renderer = Renderer::getSingleton();

	auto vs = renderer->compileShaderFromFile("shaders/imgui.hlsl", "vs", SM_VS);
	auto ps = renderer->compileShaderFromFile("shaders/imgui.hlsl", "ps", SM_PS);
	std::vector<Renderer::Shader::Ptr> shaders = { vs, ps };
	//vs->enable32BitsConstants(true);
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

	rs.setRenderTargetFormat({Renderer::BACK_BUFFER_FORMAT});

	mPipelineState = std::make_shared<Renderer::PipelineStateInstance>( rs,shaders);
	mConstants = mPipelineState->createConstantBuffer(Renderer::Shader::ST_VERTEX, "vertexBuffer");
	//mPipelineState->setVSConstant("vertexBuffer", mConstants);
}



#define GET_X_LPARAM(lp)	((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp)	((int)(short)HIWORD(lp))

//bool ImGuiOverlay::ImGuiObject::process(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
//{
//	if (ImGui::GetCurrentContext() == NULL)
//		return false;
//	auto& io = ImGui::GetIO();
//	switch (message)
//	{
//	case WM_LBUTTONDOWN: io.MouseDown[0] = true;break;
//	case WM_LBUTTONUP: io.MouseDown[0] = false; break;
//	case WM_RBUTTONDOWN: io.MouseDown[1] = true; break;
//	case WM_RBUTTONUP: io.MouseDown[1] = false; break;
//	case WM_MBUTTONDOWN: io.MouseDown[2] = true; break;
//	case WM_MBUTTONUP: io.MouseDown[2] = false; break;
//
//	case WM_MOUSEMOVE: {io.MousePos.x = (float)GET_X_LPARAM(lParam); io.MousePos.y = (float)GET_Y_LPARAM(lParam);} break;
//	}
//	return false;
//}


RenderGraph::RenderTask ImGuiPass::execute()
{
	return [this](auto cmdlist)->Future<Promise>
	{
		ImGuiPass* pass =  this;

		co_await std::suspend_always();
		

		while(!pass->mReady.load())
			co_await std::suspend_always();

		pass->mReady.store(false);
		endFrame();

		co_await std::suspend_always();

		auto data = ImGui::GetDrawData();

		if (data == NULL || data->DisplaySize.x <= 0.0f || data->DisplaySize.y <= 0.0f || data->TotalIdxCount <= 0)
		{
			beginFrame();
			co_return;
		}
		auto renderer = Renderer::getSingleton();
		auto& VertexBuffer = pass->mVertexBuffer[renderer->getCurrentFrameIndex()];
		auto& IndexBuffer = pass->mIndexBuffer[renderer->getCurrentFrameIndex()];
		if (!VertexBuffer || VertexBuffer->getSize() < data->TotalVtxCount * sizeof(ImDrawVert))
		{
			if (VertexBuffer)
				renderer->destroyResource(VertexBuffer);
			auto size = data->TotalIdxCount;
			VertexBuffer = renderer->createBuffer(size * sizeof(ImDrawVert), sizeof(ImDrawVert), false, D3D12_HEAP_TYPE_UPLOAD);
		}

		if (!IndexBuffer || IndexBuffer->getSize() < data->TotalIdxCount * sizeof(ImDrawIdx))
		{
			if (IndexBuffer)
				renderer->destroyResource(IndexBuffer);
			auto size = data->TotalIdxCount;
			IndexBuffer = renderer->createBuffer(size * sizeof(ImDrawIdx), sizeof(ImDrawIdx), false, D3D12_HEAP_TYPE_UPLOAD);
		}

		auto vertices = VertexBuffer->map(0);
		auto indices = IndexBuffer->map(0);

		for (int n = 0; n < data->CmdListsCount; n++)
		{
			const ImDrawList* cmd_list = data->CmdLists[n];
			auto numVertices = cmd_list->VtxBuffer.Size * sizeof(ImDrawVert);
			auto numIndices = cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx);
			memcpy(vertices, cmd_list->VtxBuffer.Data, numVertices);
			vertices += numVertices;
			memcpy(indices, cmd_list->IdxBuffer.Data, numIndices);
			indices += numIndices;
		}

		VertexBuffer->unmap(0);
		IndexBuffer->unmap(0);

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



		//pass->mPipelineState->setVSVariable("ProjectionMatrix", mvp);
		cmdlist->setPipelineState(pass->mPipelineState->getPipelineState());
		pass->mConstants->blit(mvp, 0, sizeof(mvp));
		cmdlist->setRootDescriptorTable(pass->mPipelineState->getConstantBufferSlot(Renderer::Shader::ST_VERTEX,"vertexBuffer"), pass->mConstants->getHandle());
		//cmdlist->set32BitConstants(1,16,mvp,0);

		D3D12_VIEWPORT vp = { 0 };
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
				if (pcmd->UserCallback == NULL)
				{
					const D3D12_RECT r = { (LONG)(pcmd->ClipRect.x - clip_off.x), (LONG)(pcmd->ClipRect.y - clip_off.y), (LONG)(pcmd->ClipRect.z - clip_off.x), (LONG)(pcmd->ClipRect.w - clip_off.y) };
					cmdlist->setScissorRect(r);
					auto handle = (D3D12_GPU_DESCRIPTOR_HANDLE*)&pcmd->TextureId;
					//pass->mPipelineState->setPSResource("texture0", *handle);
					cmdlist->setRootDescriptorTable(pass->mPipelineState->getResourceSlot(Renderer::Shader::ST_PIXEL, "texture0"), *handle);
					cmdlist->drawIndexedInstanced(pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
				}
			}
			global_idx_offset += cmd_list->IdxBuffer.Size;
			global_vtx_offset += cmd_list->VtxBuffer.Size;
		}

		beginFrame();

		co_return;
	};

}


void ImGuiPass::resize( int width, int height)
{
	auto& io = ImGui::GetIO();
	io.DisplaySize = { (float)width, (float)height };
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	//io.handl = win;

}


void ImGuiPass::initFonts()
{
	// Build texture atlas
	ImGuiIO& io = ImGui::GetIO();
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	auto renderer = Renderer::getSingleton();
	mFonts = renderer->createTexture2D(width, height, DXGI_FORMAT_R8G8B8A8_UNORM,1, pixels, false);

	static_assert(sizeof(ImTextureID) >= sizeof(mFonts->getShaderResource().ptr), "Can't pack descriptor handle into TexID, 32-bit not supported yet.");
	io.Fonts->TexID = (ImTextureID)mFonts->getShaderResource().ptr;
}

void ImGuiPass::beginFrame()
{
	HWND win = Renderer::getSingleton()->getWindow();
	RECT rect;
	::GetClientRect(win, &rect);
	resize(std::max(1L, rect.right), std::max(1L, rect.bottom));

	ImGui::NewFrame();
}

void ImGuiPass::endFrame()
{
	ImGui::Render();
}
