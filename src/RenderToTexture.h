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

	//��������
	struct objectConstant {
		UINT tWidth;	//��ͼ��
		UINT tHeight;	//��ͼ��
		float board;	//һά�ݻ��Ͷ�ά�ݻ��߽�
		UINT rule;		//�߽���Ӧ�õĹ���
	};

	bool IsVSync;		//�Ƿ����ô�ֱͬ��

	ComPtr<ID3D12CommandAllocator> m_computeCommandAllocator;
	ComPtr<ID3D12CommandQueue> m_computeCommandQueue;
	ComPtr<ID3D12RootSignature> m_computeRootSignature;
	ComPtr<ID3D12GraphicsCommandList> m_computeCommandList;
	ComPtr<ID3D12PipelineState> m_computePipelineState;		//��������PSO
	ComPtr<ID3D12PipelineState> m_computePipelineState2;	//�������״̬��PSO
	ComPtr<ID3D12DescriptorHeap> m_computeCbvSrvUavHeap;
	ComPtr<ID3D12DescriptorHeap> m_computeSamplerHeap;

	UINT m_srvUavDescriptorSize;
	static UINT colorMapSize;
	objectConstant m_constData;
	objectConstant* mMappedData = nullptr;


	ComPtr<ID3D12Fence> m_computeFences;
	UINT64 m_computeFenceValue;
	HANDLE m_computeFenceEvents;

	
	ComPtr<ID3D12Resource> m_computeTexture;		//����CS��PS
	ComPtr<ID3D12Resource> texUploadHeap;			//���ڸ�����ͼ���ݵ��ϴ���
	ComPtr<ID3D12Resource> m_uavResource[2];		//����CS���ֱ𱣴�cell���¾�״̬
	ComPtr<ID3D12Resource> m_uavUploadResource[2];	//��Ӧ�ϴ���
	ComPtr<ID3D12Resource> m_sampler;				//Ҳ����ͼ��Դ��λ��SRV�����ڸ����������������ɫ����
	ComPtr<ID3D12Resource> m_samplerUpload;			//��Ӧ�ϴ���
	ComPtr<ID3D12Resource> m_constUpload;			//�������������ϴ���


	//��ǩ���ṹ
	enum RootParameters : uint32_t
	{
		e_rootParameterCB = 0,
		e_rootParameterSampler,
		e_rootParameterSRV,
		e_rootParameterUAV,
		e_numRootParameters
	};
	// cbv_srv_uav�������ĸ�����������
	enum DescriptorHeapCount : uint32_t
	{
		e_cCB = 1,
		e_cSRV = 2,
		e_cUAV = 3,
	};
	//��������cbv_srv_uav�е����
	enum DescriptorHeapIndex : uint32_t
	{
		e_iCB = 0,
		e_iSRV = e_iCB + e_cCB,
		e_iUAV = e_iSRV + e_cSRV,
		e_iHeapEnd = e_iUAV + e_cUAV
	};

};
