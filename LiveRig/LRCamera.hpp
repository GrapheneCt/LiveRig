#pragma once

#include <kernel.h>
#include <camera.h>
#include <scetypes.h>
#include <vita2d_sys.h>

class LRCamera
{
public:

	static LRCamera *GetInstance();

	static SceVoid ReleaseInstance();

	SceInt32 Start(SceInt32 dev);

	SceInt32 Update(
		unsigned char **buf,
		SceInt32 *width,
		SceInt32 *height,
		SceUInt64 *frame,
		SceUInt64 *timestamp,
		SceBool iBufOnly = SCE_FALSE
	);

	SceInt32 Stop();

	SceInt32 GetBuffer(ScePVoid *ptr, SceInt32 *size);

	SceInt32 GetIBuffer(ScePVoid *ptr, SceInt32 *size);

	SceVoid GetSize(SceInt32 *width, SceInt32 *height);

	SceVoid SetEv(SceInt32 level);

	SceVoid DrawCamTex();

private:

	typedef enum CameraStatus {
		CAMERA_INVALID,
		CAMERA_CLOSE,
		CAMERA_OPEN,
		CAMERA_START
	} CameraStatus;

	typedef struct {
	public:
		ScePVoid		cameraBuffer;
		SceInt32		cameraBufSize;
		ScePVoid		cameraCpuIBuffer;
		SceUID			cameraCpuIBufMemblock;
		SceCameraInfo	cameraInfo;
		SceCameraRead	cameraRead;
		SceInt32		cameraDevNum;
		CameraStatus 	cameraStatus;
	} CameraCtrl;

	CameraCtrl _camCtrl;

	const SceInt32 _cameraWidth = 160;
	const SceInt32 _cameraHeight = 120;

	vita2d_texture *_tex;

	LRCamera();

	~LRCamera();
};

