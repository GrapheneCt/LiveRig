#pragma once

#include <kernel.h>
#include <gxm.h>

static const SceInt32 displayWidth = 960;
static const SceInt32 displayHeight = 544;
static const SceInt32 displayStride = 960;

class LRGXM
{
public:

	SceGxmContext *immContext;
	SceGxmRenderTarget *renderTarget;
	ScePVoid displayBufferData[2];
	SceGxmSyncObject *displayBufferSync[2];
	SceGxmColorSurface displayColorSurface[2];
	SceGxmDepthStencilSurface depthStencilSurface;
	SceGxmShaderPatcher *shaderPatcher;
	SceGxmMultisampleMode msaa;

	struct InitParams
	{
	public:
		SceUInt32				paramBufferSize;
		SceUInt32				vdmRingBufferSize;
		SceUInt32				vertexRingBufferSize;
		SceUInt32				fragmentRingBufferSize;
		SceUInt32				fragmentUsseRingBufferSize;
		SceGxmMultisampleMode	msaa;
	};

	static LRGXM *GetInstance();

	static SceVoid ReleaseInstance();

	SceInt32 Init(InitParams *params);

	SceVoid StartScene();

	SceVoid EndRendering();

	SceVoid UpdateCommonDialog();

	SceVoid EndScene();

	SceInt32 GetDisplayIndex();

	SceVoid WaitRenderingDone();

private:

	struct DisplayCallbackArg
	{
		void* address;
	};

	SceGxmValidRegion _validRegion;

	SceGxmDeviceMemInfo *_vdmRingBufferMem;
	SceGxmDeviceMemInfo *_vertexRingBufferMem;
	SceGxmDeviceMemInfo *_fragmentRingBufferMem;
	SceGxmDeviceMemInfo *_fragmentUsseRingBufferMem;
	SceGxmDeviceMemInfo *_depthBufferMem;
	SceGxmDeviceMemInfo *_stencilBufferMem;
	SceGxmDeviceMemInfo *_patcherBufferMem;
	SceGxmDeviceMemInfo *_patcherVertexUsseMem;
	SceGxmDeviceMemInfo *_patcherFragmentUsseMem;
	SceGxmDeviceMemInfo *_displayBufferMem[2];

	uint32_t _bufferIndex;

	LRGXM();

	~LRGXM();

	static void DisplayCallback(const void *callbackData);
	static void *PatcherHostAlloc(void *user_data, uint32_t size);
	static void PatcherHostFree(void *user_data, void *mem);
};
