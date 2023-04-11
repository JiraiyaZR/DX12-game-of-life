#include "stdafx.h"
#include "RenderToTexture.h"
#include <time.h>

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
	ThrowIfFailed(m_swapChain->Present(1, 0));

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

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif
		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"ComputeShader.hlsl").c_str(), nullptr, nullptr, "main", "cs_5_0", compileFlags, 0, &computeShader, nullptr));

		D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
		computePsoDesc.pRootSignature = m_computeRootSignature.Get();
		computePsoDesc.CS = CD3DX12_SHADER_BYTECODE(computeShader.Get());
		ThrowIfFailed(m_device->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&m_computePipelineState)));
		NAME_D3D12_OBJECT(m_computePipelineState);
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
			textureDesc.Width = m_width;
			textureDesc.Height = m_height;
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

			ComPtr<ID3D12Resource> texUploadHeap;
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
			m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_computeTexture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
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
			UINT totalPix = m_width * m_height;
			std::vector<LifeCell> data;
			data.resize(totalPix);
			const UINT dataSize = totalPix * sizeof(LifeCell);
			LoadData(&data[0], totalPix);

			D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(dataSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			D3D12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(dataSize);

			for (int i = 0; i < numDescriptor - 1; i++) {
				
				ThrowIfFailed(m_device->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					&bufferDesc,
					D3D12_RESOURCE_STATE_COPY_DEST,
					nullptr,
					IID_PPV_ARGS(&m_uavResource[i])));
				NAME_D3D12_OBJECT_INDEXED(m_uavResource, i);

				ThrowIfFailed(m_device->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
					D3D12_HEAP_FLAG_NONE,
					&uploadBufferDesc,
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&m_uavUploadResource[i])));
				NAME_D3D12_OBJECT_INDEXED(m_uavUploadResource, i);

				D3D12_SUBRESOURCE_DATA pixData = {};
				pixData.pData = reinterpret_cast<UINT8*>(&data[0]);
				pixData.RowPitch = dataSize;
				pixData.SlicePitch = pixData.RowPitch;

				UpdateSubresources<1>(m_computeCommandList.Get(), m_uavResource[i].Get(), m_uavUploadResource[i].Get(), 0, 0, 1, &pixData);
				m_computeCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_uavResource[i].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

				/*
				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
				srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srvDesc.Format = DXGI_FORMAT_UNKNOWN;
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				srvDesc.Buffer.FirstElement = 0;
				srvDesc.Buffer.NumElements = totalPix;
				srvDesc.Buffer.StructureByteStride = sizeof(LifeCell);
				srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

				CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_computeSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), 1 + i, m_srvUavDescriptorSize);
				m_device->CreateShaderResourceView(m_uavResource[i].Get(), &srvDesc, srvHandle);
				*/
				D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
				uavDesc.Format = DXGI_FORMAT_UNKNOWN;
				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
				uavDesc.Buffer.FirstElement = 0;
				uavDesc.Buffer.NumElements = totalPix;
				uavDesc.Buffer.StructureByteStride = sizeof(totalPix);
				uavDesc.Buffer.CounterOffsetInBytes = 0;
				uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

				CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(m_computeSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), 2 + i, m_srvUavDescriptorSize);

				m_device->CreateUnorderedAccessView(m_uavResource[i].Get(), nullptr, &uavDesc, uavHandle);
			}
			
		}

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
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_computeTexture.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

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
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_computeTexture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

	ThrowIfFailed(m_commandList->Close());
}

void RenderToTexture::populateComputeCommandList()
{
	// 转换资源
	m_computeCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_computeTexture.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
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
	m_computeCommandList->Dispatch(m_width / 8, m_height / 8, 1);

	// 转换资源使其能够被其他着色器使用
	m_computeCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_computeTexture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
	
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

	// 清空Allocator,为下一帧做准备
	ThrowIfFailed(m_computeCommandAllocator->Reset());
	ThrowIfFailed(m_computeCommandList->Reset(m_computeCommandAllocator.Get(), m_computePipelineState.Get()));

}

void RenderToTexture::CreateBuffer()
{
}

void RenderToTexture::LoadData(_Out_writes_(totalPix) LifeCell* data, UINT totalPix)
{
	srand((int)time(0));
	for (int i = 0; i < totalPix; i++) {
		if (rand() > RAND_MAX / 2) {
			data[i].state = 1;
			data[i].delay = 0;
		}
		else {
			data[i].state = 0;
			data[i].delay = 256;
		}
	}

}
