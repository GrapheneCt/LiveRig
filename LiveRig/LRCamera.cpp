#include <kernel.h>
#include <camera.h>
#include <scetypes.h>
#include <libdbg.h>
#include <gxm.h>
#include <kernel\dmacmgr.h>
#include <stdlib.h>
#include <vita2d_sys.h>

#include <target_transport.h>

#include "LRCamera.hpp"
#include "LRUtil.hpp"
#include "LRGXM.hpp"

namespace {
	LRCamera *s_instance = SCE_NULL;
}

LRCamera::LRCamera() :
	_camCtrl({0})
{
	SceInt32 ret;

	_camCtrl.cameraStatus = CAMERA_INVALID;
	_camCtrl.cameraDevNum = -1;

	//camera open
	_camCtrl.cameraInfo.sizeThis = sizeof(SceCameraInfo);
	_camCtrl.cameraInfo.wPriority = SCE_CAMERA_PRIORITY_SHARE;
	_camCtrl.cameraInfo.wFormat = SCE_CAMERA_FORMAT_YUV420_PLANE;
	_camCtrl.cameraInfo.wResolution = SCE_CAMERA_RESOLUTION_QQVGA;
	_camCtrl.cameraInfo.wFramerate = SCE_CAMERA_FRAMERATE_60;
	_camCtrl.cameraInfo.wBuffer = SCE_CAMERA_BUFFER_SETBYOPEN;

	_tex = vita2d_create_empty_texture_null();
	SceSize bufSize = ROUND_UP(_cameraWidth * _cameraHeight * 3 / 2, 0x1000);
	sceGxmAllocDeviceMemLinux(SCE_GXM_DEVICE_HEAP_ID_USER_NC, SCE_GXM_MEMORY_ATTRIB_READ, bufSize, 256, &_tex->data_mem);
	sceGxmTextureInitLinear(&_tex->gxm_tex, _tex->data_mem->mappedBase, SCE_GXM_TEXTURE_FORMAT_YUV420P2_CSC0, _cameraWidth, _cameraHeight, 0);

	_camCtrl.cameraInfo.pvIBase = _tex->data_mem->mappedBase;
	_camCtrl.cameraInfo.pvUBase = static_cast<unsigned char*>(_camCtrl.cameraInfo.pvIBase) + _cameraWidth * _cameraHeight;
	_camCtrl.cameraInfo.pvVBase = static_cast<unsigned char*>(_camCtrl.cameraInfo.pvUBase) + _cameraWidth * _cameraHeight / 4;

	_camCtrl.cameraInfo.sizeIBase = _cameraWidth * _cameraHeight;
	_camCtrl.cameraInfo.sizeUBase = _cameraWidth * _cameraHeight / 4;
	_camCtrl.cameraInfo.sizeVBase = _cameraWidth * _cameraHeight / 4;

	_camCtrl.cameraCpuIBufMemblock = sceKernelAllocMemBlock("LRCamera::IBufffer", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, ROUND_UP(_camCtrl.cameraInfo.sizeIBase, SCE_KERNEL_4KiB), SCE_NULL);
	if (_camCtrl.cameraCpuIBufMemblock <= 0)
		SCE_DBG_LOG_ERROR("[LRCamera] sceKernelAllocMemBlock() 0x%X\n", _camCtrl.cameraCpuIBufMemblock);

	sceKernelGetMemBlockBase(_camCtrl.cameraCpuIBufMemblock, &_camCtrl.cameraCpuIBuffer);

	_camCtrl.cameraInfo.wPitch = 0;
	_camCtrl.cameraStatus = CAMERA_CLOSE;

	ret = sceCameraOpen(SCE_CAMERA_DEVICE_FRONT, &_camCtrl.cameraInfo);
	if (ret < 0)
		SCE_DBG_LOG_ERROR("[LRCamera] sceCameraOpen():SCE_CAMERA_DEVICE_FRONT 0x%X\n", ret);

	/*ret = sceCameraOpen(SCE_CAMERA_DEVICE_BACK, &_camCtrl.cameraInfo);
	if (ret < 0)
		SCE_DBG_LOG_ERROR("[LRCamera] sceCameraOpen():SCE_CAMERA_DEVICE_BACK 0x%X\n", ret);*/

	_camCtrl.cameraBuffer = _tex->data_mem->mappedBase;
	_camCtrl.cameraBufSize = bufSize;

	_camCtrl.cameraStatus = CAMERA_OPEN;

	s_instance = this;
}

LRCamera::~LRCamera()
{
	SceInt32 ret;

	if (_camCtrl.cameraStatus == CAMERA_OPEN) {
		ret = sceCameraClose(SCE_CAMERA_DEVICE_FRONT);
		if (ret < 0)
			SCE_DBG_LOG_ERROR("[LRCamera] sceCameraClose(SCE_CAMERA_DEVICE_FRONT) 0x%X\n", ret);

		/*ret = sceCameraClose(SCE_CAMERA_DEVICE_BACK);
		if (ret < 0)
			SCE_DBG_LOG_ERROR("[LRCamera] sceCameraClose(SCE_CAMERA_DEVICE_BACK) 0x%X\n", ret);*/

		_camCtrl.cameraStatus = CAMERA_CLOSE;
	}

	if (_camCtrl.cameraStatus == CAMERA_CLOSE) {
		vita2d_free_texture(_tex);
		_camCtrl.cameraBuffer = SCE_NULL;
		_camCtrl.cameraBufSize = 0;
		_tex = SCE_NULL;
		sceKernelFreeMemBlock(_camCtrl.cameraCpuIBufMemblock);
		_camCtrl.cameraCpuIBufMemblock = SCE_UID_INVALID_UID;
		_camCtrl.cameraCpuIBuffer = SCE_NULL;
	}

	_camCtrl.cameraStatus = CAMERA_INVALID;
}

LRCamera *LRCamera::GetInstance()
{
	if (s_instance == SCE_NULL)
	{
		s_instance = new LRCamera();
	}

	return s_instance;
}

SceVoid LRCamera::ReleaseInstance()
{
	if (s_instance != SCE_NULL)
	{
		delete s_instance;
	}

	s_instance = SCE_NULL;
}

SceInt32 LRCamera::Start(SceInt32 dev)
{
	SceInt32 ret = 0;

	if (_camCtrl.cameraStatus != CAMERA_OPEN)
		return ret;

	//camera start
	ret = sceCameraStart(dev);
	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[LRCamera] sceCameraStart() 0x%X\n", ret);
		return ret;
	}
	_camCtrl.cameraDevNum = dev;
	_camCtrl.cameraStatus = CAMERA_START;

	_camCtrl.cameraRead.sizeThis = sizeof(SceCameraRead);

	return 0;
}

SceInt32 LRCamera::Update(
	unsigned char **buf,
	SceInt32 *width,
	SceInt32 *height,
	SceUInt64 *frame,
	SceUInt64 *timestamp,
	SceBool iBufOnly
)
{
	SceInt32 ret = 0;

	if (_camCtrl.cameraStatus != CAMERA_START)
		return ret;

	LRGXM::GetInstance()->WaitRenderingDone();
	ret = sceCameraRead(_camCtrl.cameraDevNum, &_camCtrl.cameraRead);

	if ((ret < 0) && (ret != SCE_CAMERA_ERROR_ALREADY_READ)) {
		SCE_DBG_LOG_ERROR("[LRCamera] sceCameraRead() 0x%X\n", ret);
		return ret;
	}

	if (iBufOnly) {
		sceClibMemcpy(_camCtrl.cameraCpuIBuffer, _camCtrl.cameraInfo.pvIBase, _camCtrl.cameraInfo.sizeIBase);
		*buf = static_cast<unsigned char*>(_camCtrl.cameraCpuIBuffer);
	}
	else {
		*buf = static_cast<unsigned char*>(_camCtrl.cameraInfo.pvIBase);
	}

	*width = _cameraWidth;
	*height = _cameraHeight;

	*frame = _camCtrl.cameraRead.qwFrame;
	*timestamp = _camCtrl.cameraRead.qwTimestamp;

	return ret;
}

SceInt32 LRCamera::Stop()
{
	SceInt32 ret = 0;

	if (_camCtrl.cameraStatus != CAMERA_START)
		return ret;

	ret = sceCameraStop(_camCtrl.cameraDevNum);
	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[LRCamera] sceCameraStop() 0x%X\n", ret);
		return ret;
	}
	_camCtrl.cameraDevNum = -1;
	_camCtrl.cameraStatus = CAMERA_OPEN;

	return 0;
}

SceInt32 LRCamera::GetBuffer(ScePVoid *ptr, SceInt32 *size)
{
	if (!_camCtrl.cameraBuffer) return -1;

	*ptr = _camCtrl.cameraBuffer;
	*size = _camCtrl.cameraBufSize;
	return 0;
}

SceInt32 LRCamera::GetIBuffer(ScePVoid *ptr, SceInt32 *size)
{
	if (!_camCtrl.cameraCpuIBuffer) return -1;

	*ptr = _camCtrl.cameraCpuIBuffer;
	*size = _camCtrl.cameraInfo.sizeIBase;
	return 0;
}

SceVoid LRCamera::GetSize(SceInt32 *width, SceInt32 *height)
{
	*width = _cameraWidth;
	*height = _cameraHeight;
}

SceVoid LRCamera::SetEv(SceInt32 level)
{
	if (_camCtrl.cameraStatus == CAMERA_START)
		sceCameraSetEV(_camCtrl.cameraDevNum, level);
}

SceVoid LRCamera::DrawCamTex()
{
	vita2d_draw_texture(_tex, 0, 0);	
}