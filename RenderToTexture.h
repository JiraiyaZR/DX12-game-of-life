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

private:
	void LoadComputePipeLine();
	void LoadComputeAssets();
	void PopulateCommandList();
	void populateComputeCommandList();
	void waitForCompute();
	std::vector<UINT8> LoadData();

	//常量数据
	struct objectConstant {
		UINT tWidth;	//贴图宽
		UINT tHeight;	//贴图高
		float board;	//一维演化和二维演化边界
		UINT rule;		//边界下应用的规则
	};

	bool IsVSync;		//是否启用垂直同步

	ComPtr<ID3D12CommandAllocator> m_computeCommandAllocator;
	ComPtr<ID3D12CommandQueue> m_computeCommandQueue;
	ComPtr<ID3D12RootSignature> m_computeRootSignature;
	ComPtr<ID3D12GraphicsCommandList> m_computeCommandList;
	ComPtr<ID3D12PipelineState> m_computePipelineState;		//负责计算的PSO
	ComPtr<ID3D12PipelineState> m_computePipelineState2;	//负责更新状态的PSO
	ComPtr<ID3D12DescriptorHeap> m_computeCbvSrvUavHeap;
	ComPtr<ID3D12DescriptorHeap> m_computeSamplerHeap;

	UINT m_srvUavDescriptorSize;
	static UINT colorMapSize;
	objectConstant m_constData;
	objectConstant* mMappedData = nullptr;


	ComPtr<ID3D12Fence> m_computeFences;
	UINT64 m_computeFenceValue;
	HANDLE m_computeFenceEvents;

	
	ComPtr<ID3D12Resource> m_computeTexture;		//用于CS、PS
	ComPtr<ID3D12Resource> texUploadHeap;			//用于复制贴图数据的上传堆
	ComPtr<ID3D12Resource> m_uavResource[2];		//用于CS，分别保存cell的新旧状态
	ComPtr<ID3D12Resource> m_uavUploadResource[2];	//对应上传堆
	ComPtr<ID3D12Resource> m_sampler;				//也是贴图资源，位于SRV，用于给采样器采样获得颜色数据
	ComPtr<ID3D12Resource> m_samplerUpload;			//对应上传堆
	ComPtr<ID3D12Resource> m_constUpload;			//常量缓冲区的上传堆


	//根签名结构
	enum RootParameters : uint32_t
	{
		e_rootParameterCB = 0,
		e_rootParameterSampler,
		e_rootParameterSRV,
		e_rootParameterUAV,
		e_numRootParameters
	};
	// cbv_srv_uav缓冲区的各个类型数量
	enum DescriptorHeapCount : uint32_t
	{
		e_cCB = 1,
		e_cSRV = 2,
		e_cUAV = 3,
	};
	//各类型在cbv_srv_uav中的序号
	enum DescriptorHeapIndex : uint32_t
	{
		e_iCB = 0,
		e_iSRV = e_iCB + e_cCB,
		e_iUAV = e_iSRV + e_cSRV,
		e_iHeapEnd = e_iUAV + e_cUAV
	};

};

