#include "stdafx.h"
#include "RenderToTexture.h"
#include <time.h>

byte* readBitMapFile(const std::string& filename, UINT& size) {
	FILE* filePtr; // the file pointer
	auto err = fopen_s(&filePtr, filename.c_str(), "rb");
	if (filePtr == NULL) {
		return nullptr;
	}
	BITMAPFILEHEADER bitmapFileHeader; // bitmap file header
	fread(&bitmapFileHeader, sizeof(BITMAPFILEHEADER), 1, filePtr);
	BITMAPINFOHEADER bitmapInfoHeader; // bitmap info header
	fread(&bitmapInfoHeader, sizeof(BITMAPINFOHEADER), 1, filePtr);
	int bitmapImageSize = bitmapInfoHeader.biWidth * bitmapInfoHeader.biHeight * bitmapInfoHeader.biBitCount / 8;
	size = bitmapImageSize;
	byte* f_data= new byte[bitmapImageSize];
	fseek(filePtr, bitmapFileHeader.bfOffBits, SEEK_SET);
	fread(f_data, 1, bitmapImageSize, filePtr);
	fclose(filePtr);
	return f_data;
}

RenderToTexture::RenderToTexture(UINT width, UINT height, std::wstring name): D3D12HelloTexture(width, height, name)
{
	numDescriptor = 3;
}

RenderToTexture::~RenderToTexture()
{
}

void RenderToTexture::OnInit()
{
	LoadPipeline();
	LoadAssets();
	LoadComputePipeLine();
	LoadComputeAssets();
}

void RenderToTexture::OnUpdate()
{
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
	ThrowIfFailed(m_swapChain->Present(0, 0));

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
		CD3DX12_DESCRIPTOR_RANGE ranges[2] = {};
		CD3DX12_ROOT_PARAMETER rootParameters[2] = {};

		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 3, 0);
		rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_ALL);
		
	
		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(2, rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

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
		auto hr = D3DCompileFromFile(GetAssetFullPath(L"ComputeShader.hlsl").c_str(), nullptr, nullptr, "Hmain", "cs_5_0", compileFlags, 0, &computeShader, &error);
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

		hr = D3DCompileFromFile(GetAssetFullPath(L"ComputeShader.hlsl").c_str(), nullptr, nullptr, "Vmain", "cs_5_0", compileFlags, 0, &computeShader, &error);
		if (FAILED(hr)) {
			if (&error != nullptr) {
				OutputDebugStringA((char*)error->GetBufferPointer());
			}
			ThrowIfFailed(hr);
		}
		computePsoDesc.CS = CD3DX12_SHADER_BYTECODE(computeShader.Get());
		ThrowIfFailed(m_device->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&m_computeVPipelineState)));
		NAME_D3D12_OBJECT(m_computeVPipelineState);
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
	ThrowIfFailed(m_commandAllocator->Reset());
	ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));
	// 创建相应描述符堆
	{
		D3D12_DESCRIPTOR_HEAP_DESC srvUavHeapDesc = {};
		srvUavHeapDesc.NumDescriptors = 4;
		srvUavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvUavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&srvUavHeapDesc, IID_PPV_ARGS(&m_computeSrvUavHeap)));

		m_srvUavDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
	//创建纹理
	{
		//创建纹理资源
		{
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

			// Create the GPU upload buffer.
			ThrowIfFailed(m_device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&texUploadHeap)));
			NAME_D3D12_OBJECT(texUploadHeap);

			// Copy data to the intermediate upload heap and then schedule a copy 
			// from the upload heap to the Texture2D.
			std::vector<UINT8> texture = GenerateTextureData();

			D3D12_SUBRESOURCE_DATA textureData = {};
			textureData.pData = &texture[0];
			textureData.RowPitch = TextureWidth * TexturePixelSize;
			textureData.SlicePitch = textureData.RowPitch * TextureHeight;

			UpdateSubresources(m_commandList.Get(), m_computeTexture.Get(), texUploadHeap.Get(), 0, 0, 1, &textureData);
			m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_computeTexture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		}
		
		//创建纹理资源的SRV
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Texture2D.MipLevels = 1;
			m_device->CreateShaderResourceView(m_computeTexture.Get(), &srvDesc, m_computeSrvUavHeap->GetCPUDescriptorHandleForHeapStart());
		}

		//创建纹理资源的UAV
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			uavDesc.Texture2D.MipSlice = 0;

			CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(m_computeSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), 1, m_srvUavDescriptorSize);
			m_device->CreateUnorderedAccessView(m_computeTexture.Get(), nullptr, &uavDesc, uavHandle);
		}

		//创建两个UAV的数据
		{
			D3D12_RESOURCE_DESC bufferDesc = {};
			bufferDesc.MipLevels = 1;
			bufferDesc.Format = DXGI_FORMAT_R8G8_UINT;
			bufferDesc.Width = TextureWidth;
			bufferDesc.Height = TextureHeight;
			bufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			bufferDesc.DepthOrArraySize = 1;
			bufferDesc.SampleDesc.Count = 1;
			bufferDesc.SampleDesc.Quality = 0;
			bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

			for (int i = 0; i < numDescriptor - 1; i++) {

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
				pixData.RowPitch = TextureWidth * 2;
				pixData.SlicePitch = pixData.RowPitch * TextureHeight;

				UpdateSubresources<1>(m_commandList.Get(), m_uavResource[i].Get(), m_uavUploadResource[i].Get(), 0, 0, 1, &pixData);
				m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_uavResource[i].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

				D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
				uavDesc.Format = DXGI_FORMAT_R8G8_UINT;
				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
				uavDesc.Texture2D.MipSlice = 0;

				CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(m_computeSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), 2 + i, m_srvUavDescriptorSize);

				m_device->CreateUnorderedAccessView(m_uavResource[i].Get(), nullptr, &uavDesc, uavHandle);
			}

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

	ID3D12DescriptorHeap* ppHeaps[] = { m_computeSrvUavHeap.Get() };
	m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_computeTexture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

	m_commandList->SetGraphicsRootDescriptorTable(0, m_computeSrvUavHeap->GetGPUDescriptorHandleForHeapStart());
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
	// 清空Allocator,为下一帧做准备
	ThrowIfFailed(m_computeCommandAllocator->Reset());
	ThrowIfFailed(m_computeCommandList->Reset(m_computeCommandAllocator.Get(), m_computePipelineState.Get()));
	// 设置根签名
	m_computeCommandList->SetComputeRootSignature(m_computeRootSignature.Get());
	
	// 绑定资源
	ID3D12DescriptorHeap* ppHeaps[] = { m_computeSrvUavHeap.Get() };
	m_computeCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	CD3DX12_GPU_DESCRIPTOR_HANDLE srvhandle(m_computeSrvUavHeap->GetGPUDescriptorHandleForHeapStart());
	CD3DX12_GPU_DESCRIPTOR_HANDLE uavhandle(m_computeSrvUavHeap->GetGPUDescriptorHandleForHeapStart(), 1, m_srvUavDescriptorSize);
	m_computeCommandList->SetComputeRootDescriptorTable(0, srvhandle);
	m_computeCommandList->SetComputeRootDescriptorTable(1, uavhandle);
	
	
	// 分派任务
	m_computeCommandList->Dispatch((UINT)ceilf((float)TextureWidth / 256), TextureHeight, 1);
	
	m_computeCommandList->SetPipelineState(m_computeVPipelineState.Get());

	m_computeCommandList->SetComputeRootSignature(m_computeRootSignature.Get());
	m_computeCommandList->SetComputeRootDescriptorTable(0, srvhandle);
	m_computeCommandList->SetComputeRootDescriptorTable(1, uavhandle);
	m_computeCommandList->Dispatch(TextureWidth, (UINT)ceilf((float)TextureHeight / 256), 1);
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
	srand((int)time(0));

	const UINT rowPitch = TextureWidth * 2;
	const UINT textureSize = rowPitch * TextureHeight;

	std::vector<UINT8> data(textureSize);
	UINT8* pData = &data[0];

	for (UINT n = 0; n < textureSize; n += 2)
	{
		if (n > 200*rowPitch || n < rowPitch * (TextureHeight-200)) {
			if (rand() > RAND_MAX / 2) {
				pData[n] = 0x01;        // R
				pData[n + 1] = 0xff;    // G
			}
			else {
				pData[n] = 0x00;        // R
				pData[n + 1] = 0xff;    // G
			}
		}
		else {
			pData[n] = 0x00;        // R
			pData[n + 1] = 0xff;    // G
		}
	}
	return data;
}

