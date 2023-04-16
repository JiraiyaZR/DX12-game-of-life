#include "stdafx.h"
#include "RenderToTexture.h"
#include <time.h>

void createGradientColor(std::vector<uint32_t>& data, uint32_t colorBefore, uint32_t colorAfter, int length) {
	// 从给定的颜色生成渐变色
	// 从32位数据中提取rgb分量
	byte Rb = colorBefore & 0xff;
	byte Gb = (colorBefore >> 8) & 0xff;
	byte Bb = (colorBefore >> 16) & 0xff;
	byte Ra = colorAfter & 0xff;
	byte Ga = (colorAfter >> 8) & 0xff;
	byte Ba = (colorAfter >> 16) & 0xff;
	// 计算每个分量的增量
	float Rinc = (float)(Ra - Rb) / (float)length;
	float Ginc = (float)(Ga - Gb) / (float)length;
	float Binc = (float)(Ba - Bb) / (float)length;
	// 生成渐变色
	for (int i = 0; i < length; i++) {
		uint32_t R = Rb + Rinc * i;
		uint32_t G = Gb + Ginc * i;
		uint32_t B = Bb + Binc * i;
		uint32_t color = 0xff000000 | (B << 16) | (G << 8) | R;
		data.push_back(color);
	}
}

std::vector<uint32_t> loadSamplerTex() {
	//生成数据来填充sample贴图
	//该贴图用于给采样器提供色卡
	const uint32_t colorTable[] = {0xff911c71,0xffd900ea,0xffc6bd0a,0xff7c3e13,0xff331809,0xff030100};	//颜色顺序为ABGR
	int duration[] = { 2,16,128,1024,8192 };
	std::vector<uint32_t> data;
	data.push_back(0xffffffff);
	for (int i = 0; i < 5; i++) {
		createGradientColor(data, colorTable[i], colorTable[i + 1], duration[i]);
	}
	data.push_back(colorTable[5]);
	return data;
}

RenderToTexture::RenderToTexture(UINT width, UINT height, UINT TexWidth, UINT TexHeight, std::wstring name, UINT rule, float board):
	D3D12HelloTexture(width, height, TexWidth, TexHeight,name),IsVSync(true)
{
	m_constData.rule = rule;
	m_constData.board = board;
	m_constData.tWidth = TexWidth;
	m_constData.tHeight = TexHeight;
}

RenderToTexture::~RenderToTexture()
{
	m_constUpload->Unmap(0, nullptr);
	objectConstant* mMappedData = nullptr;
}

void RenderToTexture::OnInit()
{
	//利用D3D12HelloTexture示例中的初始化
	LoadPipeline();
	LoadAssets();
	//计算流水线相关初始化
	LoadComputePipeLine();
	LoadComputeAssets();
}

void RenderToTexture::OnUpdate()
{
	//更新常量数据
	memcpy(mMappedData, &m_constData, sizeof(m_constData));
}

void RenderToTexture::OnRender()
{
	populateComputeCommandList();
	// Record all the commands we need to render the scene into the command list.
	PopulateCommandList();

	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Present the frame.
	if (IsVSync) {
		ThrowIfFailed(m_swapChain->Present(1, 0));
	}
	else {
		ThrowIfFailed(m_swapChain->Present(0, 0));
	}

	WaitForPreviousFrame();
}

void RenderToTexture::OnDestroy()
{
	// Ensure that the GPU is no longer referencing resources that are about to be
	// cleaned up by the destructor.
	WaitForPreviousFrame();

	CloseHandle(m_fenceEvent);
}

void RenderToTexture::LoadComputePipeLine()
{
	//创建根参数
	{
		CD3DX12_DESCRIPTOR_RANGE ranges[e_numRootParameters] = {};
		CD3DX12_ROOT_PARAMETER rootParameters[e_numRootParameters] = {};

		ranges[e_rootParameterCB].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);				//b(0)
		ranges[e_rootParameterSampler].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 1);		//t(1)
		ranges[e_rootParameterSRV].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);				//s(0)-s(1)
		ranges[e_rootParameterUAV].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 3, 0);				//u(0)-u(2)

		rootParameters[e_rootParameterCB].InitAsDescriptorTable(1, &ranges[e_rootParameterCB], D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[e_rootParameterSampler].InitAsDescriptorTable(1, &ranges[e_rootParameterSampler], D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[e_rootParameterSRV].InitAsDescriptorTable(1, &ranges[e_rootParameterSRV], D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[e_rootParameterUAV].InitAsDescriptorTable(1, &ranges[e_rootParameterUAV], D3D12_SHADER_VISIBILITY_ALL);
		
	
		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(e_numRootParameters, rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> serializedRootSig = nullptr;
		ComPtr<ID3DBlob> errorBlob = nullptr;
		ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSig, &errorBlob));
		ThrowIfFailed(m_device->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&m_computeRootSignature)));
		NAME_D3D12_OBJECT(m_computeRootSignature);
	}

	//创建PSO
	{
		ComPtr<ID3DBlob> computeShader;
		ComPtr<ID3DBlob> error;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif
		auto hr = D3DCompileFromFile(GetAssetFullPath(L"ComputeShader.hlsl").c_str(), nullptr, nullptr, "CSmain", "cs_5_0", compileFlags, 0, &computeShader, &error);
		if (FAILED(hr)) {
			if (&error != nullptr) {
				OutputDebugStringA((char*)error->GetBufferPointer());
			}
			ThrowIfFailed(hr);
		}

		D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
		computePsoDesc.pRootSignature = m_computeRootSignature.Get();
		computePsoDesc.CS = CD3DX12_SHADER_BYTECODE(computeShader.Get());
		ThrowIfFailed(m_device->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&m_computePipelineState)));
		NAME_D3D12_OBJECT(m_computePipelineState);

		hr = D3DCompileFromFile(GetAssetFullPath(L"ComputeShader.hlsl").c_str(), nullptr, nullptr, "stateUpdate", "cs_5_0", compileFlags, 0, &computeShader, &error);
		if (FAILED(hr)) {
			if (&error != nullptr) {
				OutputDebugStringA((char*)error->GetBufferPointer());
			}
			ThrowIfFailed(hr);
		}
		computePsoDesc.CS = CD3DX12_SHADER_BYTECODE(computeShader.Get());
		ThrowIfFailed(m_device->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&m_computePipelineState2)));
		NAME_D3D12_OBJECT(m_computePipelineState2);
	}

	//创建命令队列、分配器、列表
	{
		D3D12_COMMAND_QUEUE_DESC queueDesc = { D3D12_COMMAND_LIST_TYPE_COMPUTE,0,D3D12_COMMAND_QUEUE_FLAG_NONE };
		ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_computeCommandQueue)));
		ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&m_computeCommandAllocator)));
		ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_computeCommandAllocator.Get(), m_computePipelineState.Get(), IID_PPV_ARGS(&m_computeCommandList)));
		ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&m_computeFences)));

		m_computeFenceEvents = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (m_computeFenceEvents == nullptr)
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}
		m_computeFenceValue = 0;
	}

}

void RenderToTexture::LoadComputeAssets()
{
	//这里还会用到图形管线的命令列表，所以先重置一下
	ThrowIfFailed(m_commandAllocator->Reset());
	ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));
	// 创建相应描述符堆
	{
		D3D12_DESCRIPTOR_HEAP_DESC cbvSrvUavHeapDesc = {};
		cbvSrvUavHeapDesc.NumDescriptors = e_iHeapEnd;
		cbvSrvUavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		cbvSrvUavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&cbvSrvUavHeapDesc, IID_PPV_ARGS(&m_computeCbvSrvUavHeap)));

		m_srvUavDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
	//创建纹理
	{
		//m_computeTexture相关
		{
			//填写描述符
			D3D12_RESOURCE_DESC textureDesc = {};
			textureDesc.MipLevels = 1;
			textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			textureDesc.Width = TextureWidth;
			textureDesc.Height = TextureHeight;
			textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			textureDesc.DepthOrArraySize = 1;
			textureDesc.SampleDesc.Count = 1;
			textureDesc.SampleDesc.Quality = 0;
			textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

			ThrowIfFailed(m_device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&textureDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&m_computeTexture)));
			NAME_D3D12_OBJECT(m_computeTexture);

			const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_computeTexture.Get(), 0, 1);

			ThrowIfFailed(m_device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&texUploadHeap)));
			NAME_D3D12_OBJECT(texUploadHeap);

			//生成贴图数据
			std::vector<UINT8> texture = GenerateTextureData();

			//向上传堆中填充数据，上传堆会将数据提交到默认堆
			D3D12_SUBRESOURCE_DATA textureData = {};
			textureData.pData = &texture[0];
			textureData.RowPitch = TextureWidth * TexturePixelSize;
			textureData.SlicePitch = textureData.RowPitch * TextureHeight;

			UpdateSubresources(m_commandList.Get(), m_computeTexture.Get(), texUploadHeap.Get(), 0, 0, 1, &textureData);
			m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_computeTexture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		}
		
		//创建m_computeTexture的SRV
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Texture2D.MipLevels = 1;
			CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_computeCbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), e_iSRV, m_srvUavDescriptorSize);
			m_device->CreateShaderResourceView(m_computeTexture.Get(), &srvDesc, srvHandle);
		}

		//创建m_computeTexture的UAV
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			uavDesc.Texture2D.MipSlice = 0;

			CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(m_computeCbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), e_iUAV, m_srvUavDescriptorSize);
			m_device->CreateUnorderedAccessView(m_computeTexture.Get(), nullptr, &uavDesc, uavHandle);
		}

		//创建两个UAV的数据,对应m_uavResource[2]
		{
			D3D12_RESOURCE_DESC bufferDesc = {};
			bufferDesc.MipLevels = 1;
			bufferDesc.Format = DXGI_FORMAT_R16G16_UINT;
			bufferDesc.Width = TextureWidth;
			bufferDesc.Height = TextureHeight;
			bufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			bufferDesc.DepthOrArraySize = 1;
			bufferDesc.SampleDesc.Count = 1;
			bufferDesc.SampleDesc.Quality = 0;
			bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

			for (int i = 0; i < e_cUAV - 1; i++) {

				ThrowIfFailed(m_device->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					&bufferDesc,
					D3D12_RESOURCE_STATE_COPY_DEST,
					nullptr,
					IID_PPV_ARGS(&m_uavResource[i])));
				NAME_D3D12_OBJECT_INDEXED(m_uavResource, i);

				const UINT64 uploadBufferSize2 = GetRequiredIntermediateSize(m_uavResource[i].Get(), 0, 1);
				ThrowIfFailed(m_device->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
					D3D12_HEAP_FLAG_NONE,
					&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize2),
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&m_uavUploadResource[i])));
				NAME_D3D12_OBJECT_INDEXED(m_uavUploadResource, i);

				std::vector<UINT8> uavData = LoadData();

				D3D12_SUBRESOURCE_DATA pixData = {};
				pixData.pData = &uavData[0];
				pixData.RowPitch = TextureWidth * 4;
				pixData.SlicePitch = pixData.RowPitch * TextureHeight;

				UpdateSubresources<1>(m_commandList.Get(), m_uavResource[i].Get(), m_uavUploadResource[i].Get(), 0, 0, 1, &pixData);
				m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_uavResource[i].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

				D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
				uavDesc.Format = DXGI_FORMAT_R16G16_UINT;
				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
				uavDesc.Texture2D.MipSlice = 0;

				CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(m_computeCbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), e_iUAV + 1 + i, m_srvUavDescriptorSize);

				m_device->CreateUnorderedAccessView(m_uavResource[i].Get(), nullptr, &uavDesc, uavHandle);
			}

		}
		// 创建采样器
		{
			D3D12_SAMPLER_DESC sampler = {};
			sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
			sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			sampler.MipLODBias = 0;
			sampler.MaxAnisotropy = 0;
			sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			sampler.MinLOD = 0.0f;
			sampler.MaxLOD = D3D12_FLOAT32_MAX;

			//采样器使用的贴图m_sampler
			D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
			samplerHeapDesc.NumDescriptors = 1;
			samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			ThrowIfFailed(m_device->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&m_computeSamplerHeap)));
			m_device->CreateSampler(&sampler, m_computeSamplerHeap->GetCPUDescriptorHandleForHeapStart());
			
			//生成数据，获得贴图的大小
			auto samplerData = loadSamplerTex();
			int samplerWidth = samplerData.size();
			
			//创建资源描述符
			D3D12_RESOURCE_DESC sampleTexDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, samplerWidth, 1, 1, 1);
			
			ThrowIfFailed(m_device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&sampleTexDesc,
				D3D12_RESOURCE_STATE_COMMON,
				nullptr,
				IID_PPV_ARGS(&m_sampler)));

			const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_sampler.Get(), 0, 1);

			ThrowIfFailed(m_device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_samplerUpload)));

			//把贴图数据提交到上传堆
			D3D12_SUBRESOURCE_DATA samplerTex = {};
			samplerTex.pData = &samplerData[0];
			samplerTex.RowPitch = static_cast<LONG_PTR>(sampleTexDesc.Width * sizeof(uint32_t));
			samplerTex.SlicePitch = 1;

			m_commandList->ResourceBarrier(1,&CD3DX12_RESOURCE_BARRIER::Transition(m_sampler.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
			UpdateSubresources<1>(m_commandList.Get(), m_sampler.Get(), m_samplerUpload.Get(), 0, 0, 1, &samplerTex);
			m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_sampler.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
			
			CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_computeCbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), e_iSRV + 1, m_srvUavDescriptorSize);
			m_device->CreateShaderResourceView(m_sampler.Get(), nullptr, srvHandle);
		}
		//创建常量缓冲区
		{
			UINT cbSize = CalculateConstantBufferByteSize(static_cast<UINT>(sizeof(objectConstant)));

			m_device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(cbSize),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_constUpload));
			
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
			cbvDesc.BufferLocation = m_constUpload->GetGPUVirtualAddress();
			cbvDesc.SizeInBytes = cbSize;
			
			m_constUpload->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData));

			m_device->CreateConstantBufferView(&cbvDesc, m_computeCbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart());
		}

	}
	//执行图形队列中的命令
	{
		ThrowIfFailed(m_commandList->Close());
		ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
		m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
		// Wait until initialization is complete.
		WaitForPreviousFrame();
	}
	//执行计算队列中的命令
	{
		ThrowIfFailed(m_computeCommandList->Close());
		ID3D12CommandList* ppCommandLists[] = { m_computeCommandList.Get() };
		m_computeCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
		// Wait until initialization is complete.
		waitForCompute();
	}
}

void RenderToTexture::waitForCompute() {
	const UINT64 fence =m_computeFenceValue;
	ThrowIfFailed(m_computeCommandQueue->Signal(m_computeFences.Get(), fence));
	m_computeFenceValue++;

	// Wait until the previous frame is finished.
	if (m_computeFences->GetCompletedValue() < fence)
	{
		ThrowIfFailed(m_computeFences->SetEventOnCompletion(fence, m_fenceEvent));
		WaitForSingleObject(m_computeFenceEvents, INFINITE);
	}
}

void RenderToTexture::PopulateCommandList()
{
	// Command list allocators can only be reset when the associated 
	// command lists have finished execution on the GPU; apps should use 
	// fences to determine GPU execution progress.
	ThrowIfFailed(m_commandAllocator->Reset());

	// However, when ExecuteCommandList() is called on a particular command 
	// list, that command list can then be reset at any time and must be before 
	// re-recording.
	ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

	// Set necessary state.
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

	ID3D12DescriptorHeap* ppHeaps[] = { m_computeCbvSrvUavHeap.Get() };
	m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	//计算管道使用的资源可以在图形管道完成状态转换
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_computeTexture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
	//这里传给图形管道的SRV仍旧使用m_computeTexture资源
	CD3DX12_GPU_DESCRIPTOR_HANDLE srvhandle(m_computeCbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(), e_iSRV, m_srvUavDescriptorSize);
	//绑定到图形管道声明的槽
	m_commandList->SetGraphicsRootDescriptorTable(0, srvhandle);

	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	// Indicate that the back buffer will be used as a render target.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	// Record commands.
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	m_commandList->DrawInstanced(6, 1, 0, 0);

	// Indicate that the back buffer will now be used to present.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_computeTexture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	ThrowIfFailed(m_commandList->Close());
}

void RenderToTexture::populateComputeCommandList()
{
	// 清空Allocator
	ThrowIfFailed(m_computeCommandAllocator->Reset());
	ThrowIfFailed(m_computeCommandList->Reset(m_computeCommandAllocator.Get(), m_computePipelineState.Get()));
	// 设置根签名
	m_computeCommandList->SetComputeRootSignature(m_computeRootSignature.Get());
	
	// 绑定资源
	ID3D12DescriptorHeap* ppHeaps[] = { m_computeCbvSrvUavHeap.Get(), m_computeSamplerHeap.Get()};
	m_computeCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	CD3DX12_GPU_DESCRIPTOR_HANDLE srvhandle(m_computeCbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(),e_iSRV, m_srvUavDescriptorSize);
	CD3DX12_GPU_DESCRIPTOR_HANDLE uavhandle(m_computeCbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(), e_iUAV, m_srvUavDescriptorSize);
	CD3DX12_GPU_DESCRIPTOR_HANDLE samplerhandle(m_computeSamplerHeap->GetGPUDescriptorHandleForHeapStart());
	CD3DX12_GPU_DESCRIPTOR_HANDLE cbhandle(m_computeCbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart());
	m_computeCommandList->SetComputeRootDescriptorTable(e_rootParameterCB, cbhandle);
	m_computeCommandList->SetComputeRootDescriptorTable(e_rootParameterSRV, srvhandle);
	m_computeCommandList->SetComputeRootDescriptorTable(e_rootParameterUAV, uavhandle);
	m_computeCommandList->SetComputeRootDescriptorTable(e_rootParameterSampler, samplerhandle);
	
	// 分派任务
	m_computeCommandList->Dispatch((UINT)ceilf((float)TextureWidth / 256), TextureHeight, 1);
	// 更换PSO
	m_computeCommandList->SetPipelineState(m_computePipelineState2.Get());
	// 重新设置根参数
	m_computeCommandList->SetComputeRootSignature(m_computeRootSignature.Get());
	m_computeCommandList->SetComputeRootDescriptorTable(e_rootParameterCB, cbhandle);
	m_computeCommandList->SetComputeRootDescriptorTable(e_rootParameterSRV, srvhandle);
	m_computeCommandList->SetComputeRootDescriptorTable(e_rootParameterUAV, uavhandle);
	m_computeCommandList->Dispatch((UINT)ceilf((float)TextureWidth / 256), TextureHeight, 1);
	// 换回原来的PSO
	m_computeCommandList->SetPipelineState(m_computePipelineState.Get());
	
	// 关闭命令列表并执行
	ThrowIfFailed(m_computeCommandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_computeCommandList.Get() };
	m_computeCommandQueue->ExecuteCommandLists(1, ppCommandLists);

	// 等待计算着色器完成任务
	m_computeFenceValue++;
	UINT64 currentFenceValue = m_computeFenceValue;
	ThrowIfFailed(m_computeCommandQueue->Signal(m_computeFences.Get(), currentFenceValue));
	ThrowIfFailed(m_computeFences->SetEventOnCompletion(currentFenceValue, m_computeFenceEvents));
	WaitForSingleObject(m_computeFenceEvents, INFINITE);
}


std::vector<UINT8> RenderToTexture::LoadData()
{
	// srand((int)time(0));

	const UINT rowPitch = TextureWidth * 4;
	const UINT textureSize = rowPitch * TextureHeight;

	std::vector<UINT8> data(textureSize);
	UINT8* pData = &data[0];
	
	//贴图数据全部置零
	for (UINT n = 0; n < textureSize; n += 4)
	{
		pData[n] = 0x00;        // R
		pData[n + 1] = 0x00;	// R
		pData[n + 2] = 0xff;    // G
		pData[n + 3] = 0xff;    // G
	}
	//在窗口最后一行的中间放置一个活cell
	int k = rowPitch * (TextureHeight - 1) + rowPitch / 2;
	pData[k] = 0x01;
	pData[k + 1] = 0x00;
	pData[k + 2] = 0xff;
	pData[k + 3] = 0xff;
	
	return data;
}


void RenderToTexture::OnKeyDown(UINT8 key)
{
	switch (key)
	{
	case VK_SPACE:
		//空格键更改垂直同步设置
		IsVSync = !IsVSync;
		break;
	case VK_ESCAPE:
		//退出
		PostQuitMessage(0);
		break;
	}
}