#pragma once
#include "D3D12HelloTexture.h"

class RenderToTexture : public D3D12HelloTexture
{
public:
	RenderToTexture(UINT width, UINT height, std::wstring name);
	virtual ~RenderToTexture();
	virtual void OnInit();
	virtual void OnUpdate();
	virtual void OnRender();
	virtual void OnDestroy();

protected:
	enum ComputeRootParameters :UINT32 {
		ComputeRootSRVTable = 0,
		ComputeRootUAVTable = 3,
		ComputeRootParameterCount = 6
	};

	enum DescriptorHeapIndex :UINT32 {
		i_UAV = 0,
		i_SRV = i_UAV + 3,
		DescriptorCount = i_SRV + 3
	};

	struct LifeCell {
		UINT state;
		UINT delay;
	};

	void LoadComputePipeLine();
	void LoadComputeAssets();
	void PopulateCommandList();
	void populateComputeCommandList();
	void CreateBuffer();
	void LoadData(_Out_writes_(totalPix) LifeCell* data, UINT totalPix);


	ComPtr<ID3D12CommandAllocator> m_computeCommandAllocator;
	ComPtr<ID3D12CommandQueue> m_computeCommandQueue;
	ComPtr<ID3D12RootSignature> m_computeRootSignature;
	ComPtr<ID3D12GraphicsCommandList> m_computeCommandList;
	ComPtr<ID3D12PipelineState> m_computePipelineState;
	ComPtr<ID3D12DescriptorHeap> m_computeSrvUavHeap;
	ComPtr<ID3D12DescriptorHeap> m_computeUavHeap;
	UINT m_srvUavDescriptorSize;
	UINT numDescriptor;

	ComPtr<ID3D12Fence> m_computeFences;
	UINT64 m_computeFenceValue;
	HANDLE m_computeFenceEvents;

	
	ComPtr<ID3D12Resource> m_computeTexture;
	ComPtr<ID3D12Resource> m_uavResource[2];
	ComPtr<ID3D12Resource> m_uavUploadResource[2];
	


};

