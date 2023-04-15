#pragma once
#include "D3D12HelloTexture.h"

class RenderToTexture : public D3D12HelloTexture
{
public:
	RenderToTexture(UINT width, UINT height, UINT TexWidth, UINT TexHeight, std::wstring name, UINT rule = 30, float board = 0.8);
	virtual ~RenderToTexture();
	virtual void OnInit();
	virtual void OnUpdate();
	virtual void OnRender();
	virtual void OnDestroy();
	virtual void OnKeyDown(UINT8 key);

protected:

	void LoadComputePipeLine();
	void LoadComputeAssets();
	void PopulateCommandList();
	void populateComputeCommandList();
	void waitForCompute();
	std::vector<UINT8> LoadData();

	struct lifeCell {
		bool isAlive;
		UINT16 delay;
	};

	//常量数据
	struct objectConstant {
		UINT tWidth;	//贴图宽
		UINT tHeight;	//贴图高
		float board;		//一维演化和二维演化边界
		UINT rule;		//边界下应用的规则
	};

	bool IsVSync;

	ComPtr<ID3D12CommandAllocator> m_computeCommandAllocator;
	ComPtr<ID3D12CommandQueue> m_computeCommandQueue;
	ComPtr<ID3D12RootSignature> m_computeRootSignature;
	ComPtr<ID3D12GraphicsCommandList> m_computeCommandList;
	ComPtr<ID3D12PipelineState> m_computePipelineState;
	ComPtr<ID3D12PipelineState> m_computeVPipelineState;
	ComPtr<ID3D12DescriptorHeap> m_computeCbvSrvUavHeap;
	ComPtr<ID3D12DescriptorHeap> m_computeSamplerHeap;
	ComPtr<ID3D12DescriptorHeap> m_constHeap;

	UINT m_srvUavDescriptorSize;
	static UINT colorMapSize;
	objectConstant m_constData;
	objectConstant* mMappedData = nullptr;


	ComPtr<ID3D12Fence> m_computeFences;
	UINT64 m_computeFenceValue;
	HANDLE m_computeFenceEvents;

	
	ComPtr<ID3D12Resource> m_computeTexture; 
	ComPtr<ID3D12Resource> texUploadHeap;
	ComPtr<ID3D12Resource> m_uavResource[2];
	ComPtr<ID3D12Resource> m_uavUploadResource[2];
	ComPtr<ID3D12Resource> m_sampler;
	ComPtr<ID3D12Resource> m_samplerUpload;
	ComPtr<ID3D12Resource> m_constUpload;



	enum RootParameters : uint32_t
	{
		e_rootParameterCB = 0,
		e_rootParameterSampler,
		e_rootParameterSRV,
		e_rootParameterUAV,
		e_numRootParameters
	};

	enum DescriptorHeapCount : uint32_t
	{
		e_cCB = 1,
		e_cSRV = 2,
		e_cUAV = 3,
	};

	enum DescriptorHeapIndex : uint32_t
	{
		e_iCB = 0,
		e_iSRV = e_iCB + e_cCB,
		e_iUAV = e_iSRV + e_cSRV,
		e_iHeapEnd = e_iUAV + e_cUAV
	};

};

