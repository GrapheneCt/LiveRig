#include <stdlib.h>
#include <kernel.h>
#include <display.h>
#include <gxm.h>
#include <libdbg.h>
#include <message_dialog.h>
#include <vita2d_sys.h>

#include "LRGXM.hpp"
#include "LRUtil.hpp"

namespace {
	LRGXM *s_instance = SCE_NULL;
}

LRGXM *LRGXM::GetInstance()
{
	if (s_instance == SCE_NULL)
	{
		s_instance = new LRGXM();
	}

	return s_instance;
}

SceVoid LRGXM::ReleaseInstance()
{
	if (s_instance != SCE_NULL)
	{
		delete s_instance;
	}

	s_instance = SCE_NULL;
}

LRGXM::LRGXM()
{
	s_instance = this;
}

LRGXM::~LRGXM()
{
	SCE_DBG_LOG_WARNING("[LRGXM] GXM termination is not implemented");
}

SceInt32 LRGXM::Init(InitParams *params)
{
	SceInt32 err;

	if (params == SCE_NULL) {
		return -1;
	}

	if (!params->paramBufferSize)
		params->paramBufferSize = SCE_GXM_DEFAULT_PARAMETER_BUFFER_SIZE;
	if (!params->vdmRingBufferSize)
		params->vdmRingBufferSize = SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE;
	if (!params->vertexRingBufferSize)
		params->vertexRingBufferSize = SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE;
	if (!params->fragmentRingBufferSize)
		params->fragmentRingBufferSize = SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE;
	if (!params->fragmentUsseRingBufferSize)
		params->fragmentUsseRingBufferSize = SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE;

	SceGxmInitializeParams initializeParams;
	sceClibMemset(&initializeParams, 0, sizeof(SceGxmInitializeParams));
	initializeParams.flags = 0;
	initializeParams.displayQueueMaxPendingCount = 2;
	initializeParams.displayQueueCallback = LRGXM::DisplayCallback;
	initializeParams.displayQueueCallbackDataSize = sizeof(DisplayCallbackArg);
	initializeParams.parameterBufferSize = params->paramBufferSize;

	err = sceGxmInitialize(&initializeParams);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("[LRGXM] sceGxmInitialize(): 0x%X", err);
		return err;
	}

	_validRegion.xMax = displayWidth - 1;
	_validRegion.yMax = displayHeight - 1;

	msaa = params->msaa;

	// allocate ring buffer memory

	err = sceGxmAllocDeviceMemLinux(
		SCE_GXM_DEVICE_HEAP_ID_USER_NC,
		SCE_GXM_MEMORY_ATTRIB_READ,
		params->vdmRingBufferSize,
		4,
		&_vdmRingBufferMem);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("[LRGXM] VDM ring buffer allocation failed: 0x%X", err);
		return err;
	}

	err = sceGxmAllocDeviceMemLinux(
		SCE_GXM_DEVICE_HEAP_ID_USER_NC,
		SCE_GXM_MEMORY_ATTRIB_READ,
		params->vertexRingBufferSize,
		4,
		&_vertexRingBufferMem);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("[LRGXM] Vertex ring buffer allocation failed: 0x%X", err);
		return err;
	}

	err = sceGxmAllocDeviceMemLinux(
		SCE_GXM_DEVICE_HEAP_ID_USER_NC,
		SCE_GXM_MEMORY_ATTRIB_READ,
		params->fragmentRingBufferSize,
		4,
		&_fragmentRingBufferMem);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("[LRGXM] Fragment ring buffer allocation failed: 0x%X", err);
		return err;
	}

	err = sceGxmAllocDeviceMemLinux(
		SCE_GXM_DEVICE_HEAP_ID_FRAGMENT_USSE,
		SCE_GXM_MEMORY_ATTRIB_READ,
		params->fragmentUsseRingBufferSize,
		4096,
		&_fragmentUsseRingBufferMem);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("[LRGXM] Fragment USSE ring buffer allocation failed: 0x%X", err);
		return err;
	}

	SceGxmContextParams contextParams;
	sceClibMemset(&contextParams, 0, sizeof(SceGxmContextParams));
	contextParams.hostMem = malloc(SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE);
	contextParams.hostMemSize = SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE;
	contextParams.vdmRingBufferMem = _vdmRingBufferMem->mappedBase;
	contextParams.vdmRingBufferMemSize = params->vdmRingBufferSize;
	contextParams.vertexRingBufferMem = _vertexRingBufferMem->mappedBase;
	contextParams.vertexRingBufferMemSize = params->vertexRingBufferSize;
	contextParams.fragmentRingBufferMem = _fragmentRingBufferMem->mappedBase;
	contextParams.fragmentRingBufferMemSize = params->fragmentRingBufferSize;
	contextParams.fragmentUsseRingBufferMem = _fragmentUsseRingBufferMem->mappedBase;
	contextParams.fragmentUsseRingBufferMemSize = params->fragmentUsseRingBufferSize;
	contextParams.fragmentUsseRingBufferOffset = _fragmentUsseRingBufferMem->offset;

	err = sceGxmCreateContext(&contextParams, &immContext);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("[LRGXM] sceGxmCreateContext(): 0x%X", err);
		return err;
	}

	// set up parameters
	SceGxmRenderTargetParams renderTargetParams;
	sceClibMemset(&renderTargetParams, 0, sizeof(SceGxmRenderTargetParams));
	renderTargetParams.flags = 0;
	renderTargetParams.width = displayWidth;
	renderTargetParams.height = displayHeight;
	renderTargetParams.scenesPerFrame = 2;
	renderTargetParams.multisampleMode = msaa;
	renderTargetParams.multisampleLocations = 0;
	renderTargetParams.driverMemBlock = SCE_UID_INVALID_UID;

	// create the render target
	err = sceGxmCreateRenderTarget(&renderTargetParams, &renderTarget);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("[LRGXM] sceGxmCreateRenderTarget(): 0x%X", err);
		return err;
	}

	// allocate memory and sync objects for display buffers
	for (int i = 0; i < 2; i++) {

		// allocate memory for display
		sceGxmAllocDeviceMemLinux(
			SCE_GXM_DEVICE_HEAP_ID_CDRAM,
			SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
			4 * displayStride * displayHeight,
			SCE_GXM_COLOR_SURFACE_ALIGNMENT,
			&_displayBufferMem[i]);

		displayBufferData[i] = _displayBufferMem[i]->mappedBase;

		// memset the buffer to black
		sceGxmTransferFill(
			0xFF000000,
			SCE_GXM_TRANSFER_FORMAT_U8U8U8U8_ABGR,
			displayBufferData[i],
			0,
			0,
			displayWidth,
			displayHeight,
			displayStride,
			SCE_NULL,
			0,
			SCE_NULL);

		// create a sync object that we will associate with this buffer
		err = sceGxmSyncObjectCreate(&displayBufferSync[i]);

		if (err != SCE_OK) {
			SCE_DBG_LOG_ERROR("[LRGXM] sceGxmSyncObjectCreate(): 0x%X", err);
			return err;
		}

		// initialize a color surface for this display buffer
		err = sceGxmColorSurfaceInit(
			&displayColorSurface[i],
			SCE_GXM_COLOR_FORMAT_A8B8G8R8,
			SCE_GXM_COLOR_SURFACE_LINEAR,
			(msaa == SCE_GXM_MULTISAMPLE_NONE) ? SCE_GXM_COLOR_SURFACE_SCALE_NONE : SCE_GXM_COLOR_SURFACE_SCALE_MSAA_DOWNSCALE,
			SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
			displayWidth,
			displayHeight,
			displayStride,
			displayBufferData[i]);

		if (err != SCE_OK) {
			SCE_DBG_LOG_ERROR("[LRGXM] sceGxmColorSurfaceInit(): 0x%X", err);
			return err;
		}
	}

	// compute the memory footprint of the depth buffer
	const uint32_t alignedWidth = ROUND_UP(displayWidth, SCE_GXM_TILE_SIZEX);
	const uint32_t alignedHeight = ROUND_UP(displayHeight, SCE_GXM_TILE_SIZEY);
	uint32_t sampleCount = alignedWidth * alignedHeight;
	uint32_t depthStrideInSamples = alignedWidth;
	if (msaa == SCE_GXM_MULTISAMPLE_4X) {
		// samples increase in X and Y
		sampleCount *= 4;
		depthStrideInSamples *= 2;
	}
	else if (msaa == SCE_GXM_MULTISAMPLE_2X) {
		// samples increase in Y only
		sampleCount *= 2;
	}

	// allocate the depth buffer
	err = sceGxmAllocDeviceMemLinux(
		SCE_GXM_DEVICE_HEAP_ID_CDRAM,
		SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
		4 * sampleCount,
		SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT,
		&_depthBufferMem);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("[LRGXM] sceGxmAllocDeviceMemLinux(): 0x%X", err);
		return err;
	}

	// allocate the stencil buffer
	err = sceGxmAllocDeviceMemLinux(
		SCE_GXM_DEVICE_HEAP_ID_CDRAM,
		SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
		4 * sampleCount,
		SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT,
		&_stencilBufferMem);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("[LRGXM] sceGxmAllocDeviceMemLinux(): 0x%X", err);
		return err;
	}

	// create the SceGxmDepthStencilSurface structure
	err = sceGxmDepthStencilSurfaceInit(
		&depthStencilSurface,
		SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24,
		SCE_GXM_DEPTH_STENCIL_SURFACE_TILED,
		depthStrideInSamples,
		_depthBufferMem->mappedBase,
		_stencilBufferMem->mappedBase);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("[LRGXM] sceGxmDepthStencilSurfaceInit(): 0x%X", err);
		return err;
	}

	// set the stencil test reference (this is currently assumed to always remain 1 after here for region clipping)
	sceGxmSetFrontStencilRef(immContext, 1);

	// set the stencil function (this wouldn't actually be needed, as the set clip rectangle function has to call this at the begginning of every scene)
	sceGxmSetFrontStencilFunc(
		immContext,
		SCE_GXM_STENCIL_FUNC_ALWAYS,
		SCE_GXM_STENCIL_OP_KEEP,
		SCE_GXM_STENCIL_OP_KEEP,
		SCE_GXM_STENCIL_OP_KEEP,
		0xFF,
		0xFF);

	// set buffer sizes for this sample
	const uint32_t patcherBufferSize = 64 * 1024;
	const uint32_t patcherVertexUsseSize = 64 * 1024;
	const uint32_t patcherFragmentUsseSize = 64 * 1024;

	// allocate memory for buffers and USSE code
	err = sceGxmAllocDeviceMemLinux(
		SCE_GXM_DEVICE_HEAP_ID_USER_NC,
		SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
		patcherBufferSize,
		4,
		&_patcherBufferMem);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("[LRGXM] sceGxmAllocDeviceMemLinux(): 0x%X", err);
		return err;
	}

	err = sceGxmAllocDeviceMemLinux(
		SCE_GXM_DEVICE_HEAP_ID_VERTEX_USSE,
		SCE_GXM_MEMORY_ATTRIB_READ,
		patcherVertexUsseSize,
		4096,
		&_patcherVertexUsseMem);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("[LRGXM] sceGxmAllocDeviceMemLinux(): 0x%X", err);
		return err;
	}

	err = sceGxmAllocDeviceMemLinux(
		SCE_GXM_DEVICE_HEAP_ID_FRAGMENT_USSE,
		SCE_GXM_MEMORY_ATTRIB_READ,
		patcherFragmentUsseSize,
		4096,
		&_patcherFragmentUsseMem);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("[LRGXM] sceGxmAllocDeviceMemLinux(): 0x%X", err);
		return err;
	}

	// create a shader patcher
	SceGxmShaderPatcherParams patcherParams;
	sceClibMemset(&patcherParams, 0, sizeof(SceGxmShaderPatcherParams));
	patcherParams.userData = SCE_NULL;
	patcherParams.hostAllocCallback = &PatcherHostAlloc;
	patcherParams.hostFreeCallback = &PatcherHostFree;
	patcherParams.bufferAllocCallback = SCE_NULL;
	patcherParams.bufferFreeCallback = SCE_NULL;
	patcherParams.bufferMem = _patcherBufferMem->mappedBase;
	patcherParams.bufferMemSize = _patcherBufferMem->size;
	patcherParams.vertexUsseAllocCallback = SCE_NULL;
	patcherParams.vertexUsseFreeCallback = SCE_NULL;
	patcherParams.vertexUsseMem = _patcherVertexUsseMem->mappedBase;
	patcherParams.vertexUsseMemSize = _patcherVertexUsseMem->size;
	patcherParams.vertexUsseOffset = _patcherVertexUsseMem->offset;
	patcherParams.fragmentUsseAllocCallback = SCE_NULL;
	patcherParams.fragmentUsseFreeCallback = SCE_NULL;
	patcherParams.fragmentUsseMem = _patcherFragmentUsseMem->mappedBase;
	patcherParams.fragmentUsseMemSize = _patcherFragmentUsseMem->size;
	patcherParams.fragmentUsseOffset = _patcherFragmentUsseMem->offset;

	err = sceGxmShaderPatcherCreate(&patcherParams, &shaderPatcher);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("[LRGXM] sceGxmShaderPatcherCreate(): 0x%X", err);
		return err;
	}

	sceGxmSetViewportEnable(immContext, SCE_GXM_VIEWPORT_ENABLED);

	vita2d_init_param_external v2dParam;
	sceClibMemset(&v2dParam, 0, sizeof(vita2d_init_param_external));
	v2dParam.imm_context = immContext;
	v2dParam.render_target = renderTarget;
	v2dParam.shader_patcher = shaderPatcher;
	v2dParam.display_stride = displayStride;
	v2dParam.display_color_surface[0] = &displayColorSurface[0];
	v2dParam.display_color_surface[1] = &displayColorSurface[1];
	v2dParam.display_buffer_sync[0] = displayBufferSync[0];
	v2dParam.display_buffer_sync[1] = displayBufferSync[1];
	v2dParam.display_buffer_data[0] = displayBufferData[0];
	v2dParam.display_buffer_data[1] = displayBufferData[1];
	v2dParam.depth_stencil_surface = &depthStencilSurface;
	v2dParam.msaa = msaa;

	vita2d_display_set_max_resolution(displayWidth, displayHeight);
	vita2d_display_set_resolution(displayWidth, displayHeight);

	vita2d_init_external(&v2dParam);
	vita2d_set_clear_color(RGBA8(255, 255, 255, 255));

	s_instance = this;

	return SCE_OK;
}

void LRGXM::DisplayCallback(const void *callbackData)
{
	const DisplayCallbackArg* arg = (const DisplayCallbackArg*)callbackData;
	SceDisplayFrameBuf framebuf;

	sceClibMemset(&framebuf, 0, sizeof(SceDisplayFrameBuf));
	framebuf.size = sizeof(SceDisplayFrameBuf);
	framebuf.base = arg->address;
	framebuf.pitch = displayStride;
	framebuf.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
	framebuf.width = displayWidth;
	framebuf.height = displayHeight;
	sceDisplaySetFrameBuf(&framebuf, SCE_DISPLAY_UPDATETIMING_NEXTVSYNC);

	sceDisplayWaitVblankStart();
}

void *LRGXM::PatcherHostAlloc(void *user_data, uint32_t size)
{
	void* pMem = malloc(size);
	if (pMem == SCE_NULL)
		SCE_DBG_LOG_ERROR("[LRGXM] patcherHostAlloc(): malloc() returned NULL");
	return pMem;
}

void LRGXM::PatcherHostFree(void *user_data, void *mem)
{
	free(mem);
}

SceVoid LRGXM::StartScene()
{
	sceGxmBeginScene(
		immContext,
		0,
		renderTarget,
		&_validRegion,
		SCE_NULL,
		displayBufferSync[_bufferIndex],
		&displayColorSurface[_bufferIndex],
		&depthStencilSurface);
}

SceVoid LRGXM::EndScene()
{
	sceGxmEndScene(immContext, NULL, NULL);
}

SceVoid LRGXM::UpdateCommonDialog()
{
	SceCommonDialogUpdateParam updateParam;
	sceClibMemset(&updateParam, 0, sizeof(updateParam));

	updateParam.renderTarget.colorFormat = SCE_GXM_COLOR_FORMAT_A8B8G8R8;
	updateParam.renderTarget.surfaceType = SCE_GXM_COLOR_SURFACE_LINEAR;
	updateParam.renderTarget.width = displayWidth;
	updateParam.renderTarget.height = displayHeight;
	updateParam.renderTarget.strideInPixels = displayStride;

	updateParam.renderTarget.colorSurfaceData = displayBufferData[_bufferIndex];
	updateParam.renderTarget.depthSurfaceData = _depthBufferMem->mappedBase;
	updateParam.displaySyncObject = displayBufferSync[_bufferIndex];

	sceCommonDialogUpdate(&updateParam);
}

SceVoid LRGXM::EndRendering()
{
	sceGxmPadHeartbeat(&displayColorSurface[_bufferIndex], displayBufferSync[_bufferIndex]);

	int oldFb = (_bufferIndex + 1) % 2;

	DisplayCallbackArg displayData;
	displayData.address = displayBufferData[_bufferIndex];
	sceGxmDisplayQueueAddEntry(
		displayBufferSync[oldFb],
		displayBufferSync[_bufferIndex],
		&displayData);

	_bufferIndex = (_bufferIndex + 1) % 2;
}

SceInt32 LRGXM::GetDisplayIndex()
{
	return _bufferIndex;
}

SceVoid LRGXM::WaitRenderingDone()
{
	sceGxmFinish(immContext);
}