#include <kernel.h>
#include <libdbg.h>
#include <libsysmodule.h>
#include <libface.h>
#include <stdlib.h>
#include <math.h>
#include <display.h>
#include <scetypes.h>
#include <vita2d_sys.h>

#include <target_transport.h>

#include "LRFace.hpp"
#include "LRCamera.hpp"
#include "LRGXM.hpp"
#include "LRUtil.hpp"

namespace {
	LRFace *s_instance = SCE_NULL;
}

LRFace *LRFace::GetInstance()
{
	if (s_instance == SCE_NULL)
	{
		s_instance = new LRFace();
	}

	return s_instance;
}

SceVoid LRFace::ReleaseInstance()
{
	if (s_instance != SCE_NULL)
	{
		delete s_instance;
	}

	s_instance = SCE_NULL;
}

LRFace::LRFace() :
	_prevFrame(0),
	_evCalibrationNum(0),
	_waitFrameCount(0),
	_evLevel(0),
	_isTracking(SCE_FALSE),
	_isShapeTrack(SCE_TRUE),
	_lostThres(SCE_FACE_SHAPE_SCORE_LOST_THRES_DEFAULT),
	_evScore{0.f}
{
	SceInt32 ret;

	sceClibMemset(&_shapeData, 0, sizeof(SceFaceShapeResult));

	sceKernelCreateLwMutex(&_faceMtx, "LRFace:FaceMtx", SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE, 0, NULL);

	// Load face module
	ret = sceSysmoduleLoadModule(SCE_SYSMODULE_FACE);
	if (ret != SCE_OK)
		SCE_DBG_LOG_ERROR("[LRFace] sceSysmoduleLoadModule(SCE_SYSMODULE_FACE) 0x%X\n", ret);

	// open dictionary file...
	// face detect
	_detectDictPtr = (SceUInt8 *)malloc(SCE_FACE_DETECT_ROLL_YAW_PITCH_DICT_SIZE);
	if (_detectDictPtr == NULL)
		SCE_DBG_LOG_ERROR("[LRFace] malloc() failed.\n");

	SceUID fd = sceIoOpen("app0:sce_data/libface/" SCE_FACE_DETECT_ROLL_YAW_PITCH_DICT, SCE_O_RDONLY, 0);
	if (fd > 0) {
		sceIoRead(fd, _detectDictPtr, SCE_FACE_DETECT_ROLL_YAW_PITCH_DICT_SIZE);
		sceIoClose(fd);
	}
	else
		SCE_DBG_LOG_ERROR("[LRFace] detect dict not found\n");

	//face detect local
	_detectLocalDictPtr = (uint8_t*)malloc(SCE_FACE_DETECT_ROLL_YAW_DICT_SIZE);
	if (_detectLocalDictPtr == NULL)
		SCE_DBG_LOG_ERROR("[LRFace] malloc() failed.\n");

	fd = sceIoOpen("app0:sce_data/libface/" SCE_FACE_DETECT_ROLL_YAW_DICT, SCE_O_RDONLY, 0);
	if (fd > 0) {
		sceIoRead(fd, _detectLocalDictPtr, SCE_FACE_DETECT_ROLL_YAW_DICT_SIZE);
		sceIoClose(fd);
	}
	else
		SCE_DBG_LOG_ERROR("[LRFace] detect-local dict not found\n");

	// parts detect
	_partsDictPtr = (uint8_t*)malloc(SCE_FACE_PARTS_ROLL_YAW_DICT_SIZE);
	if (_partsDictPtr == NULL)
		SCE_DBG_LOG_ERROR("[LRFace] malloc() failed.\n");

	fd = sceIoOpen("app0:sce_data/libface/" SCE_FACE_PARTS_ROLL_YAW_DICT, SCE_O_RDONLY, 0);
	if (fd > 0) {
		sceIoRead(fd, _partsDictPtr, SCE_FACE_PARTS_ROLL_YAW_DICT_SIZE);
		sceIoClose(fd);
	}
	else
		SCE_DBG_LOG_ERROR("[LRFace] parts dict not found\n");

	// allparts detect
	_allPartsDictPtr = (uint8_t*)malloc(SCE_FACE_ALLPARTS_DICT_SIZE);
	if (_allPartsDictPtr == NULL)
		SCE_DBG_LOG_ERROR("[LRFace] malloc() failed.\n");

	fd = sceIoOpen("app0:sce_data/libface/" SCE_FACE_ALLPARTS_DICT, SCE_O_RDONLY, 0);
	if (fd > 0) {
		sceIoRead(fd, _allPartsDictPtr, SCE_FACE_ALLPARTS_DICT_SIZE);
		sceIoClose(fd);
	}
	else
		SCE_DBG_LOG_ERROR("[LRFace] parts dict not found\n");

	// parts checker
	_partsCheckDictPtr = (uint8_t*)malloc(SCE_FACE_PARTS_CHECK_DICT_SIZE);
	if (_partsCheckDictPtr == NULL)
		SCE_DBG_LOG_ERROR("[LRFace] malloc() failed.\n");

	fd = sceIoOpen("app0:sce_data/libface/" SCE_FACE_PARTS_CHECK_DICT, SCE_O_RDONLY, 0);
	if (fd > 0) {
		sceIoRead(fd, _partsCheckDictPtr, SCE_FACE_PARTS_CHECK_DICT_SIZE);
		sceIoClose(fd);
	}
	else
		SCE_DBG_LOG_ERROR("[LRFace] parts check dict not found\n");

	// attrib classify
	_attribDictPtr = (SceFaceAttribDictPtr)malloc(SCE_FACE_ATTRIB_DICT_SIZE);
	if (_attribDictPtr == NULL)
		SCE_DBG_LOG_ERROR("[LRFace] malloc() failed.\n");

	fd = sceIoOpen("app0:sce_data/libface/" SCE_FACE_ATTRIB_DICT, SCE_O_RDONLY, 0);
	if (fd > 0) {
		sceIoRead(fd, _attribDictPtr, SCE_FACE_ATTRIB_DICT_SIZE);
		sceIoClose(fd);
	}
	else
		SCE_DBG_LOG_ERROR("[LRFace] attrib dict not found\n");

	//shape
	_shapeDictPtr = (SceFaceShapeModelDictPtr)malloc(SCE_FACE_SHAPE_DICT_FRONTAL_SIZE);
	if (_shapeDictPtr == NULL)
		SCE_DBG_LOG_ERROR("[LRFace] malloc() failed.\n");

	fd = sceIoOpen("app0:sce_data/libface/" SCE_FACE_SHAPE_DICT_FRONTAL, SCE_O_RDONLY, 0);
	if (fd > 0) {
		sceIoRead(fd, _shapeDictPtr, SCE_FACE_SHAPE_DICT_FRONTAL_SIZE);
		sceIoClose(fd);
	}
	else
		SCE_DBG_LOG_ERROR("[LRFace] shape dict not found\n");

	//allparts shape
	_shapeApDictPtr = (SceFaceShapeDictPtr)malloc(SCE_FACE_ALLPARTS_SHAPE_DICT_SIZE);
	if (_shapeApDictPtr == NULL)
		SCE_DBG_LOG_ERROR("[LRFace] malloc() failed.\n");

	fd = sceIoOpen("app0:sce_data/libface/" SCE_FACE_ALLPARTS_SHAPE_DICT, SCE_O_RDONLY, 0);
	if (fd > 0) {
		sceIoRead(fd, _shapeApDictPtr, SCE_FACE_ALLPARTS_SHAPE_DICT_SIZE);
		sceIoClose(fd);
	}
	else
		SCE_DBG_LOG_ERROR("[LRFace] shape dict not found\n");

	SceInt32 cameraWidth, cameraHeight;
	LRCamera *cam = LRCamera::GetInstance();

	cam->GetSize(&cameraWidth, &cameraHeight);

	_workSize = sceFaceDetectionGetWorkingMemorySize(cameraWidth, cameraHeight, cameraWidth, _detectDictPtr);
	_workPtr = malloc(_workSize);

	_workSizeLocal = sceFaceDetectionGetWorkingMemorySize(cameraWidth, cameraHeight, cameraWidth, _detectLocalDictPtr);
	_workPtrLocal = malloc(_workSizeLocal);

	_workSizeParts = sceFacePartsGetWorkingMemorySize(cameraWidth, cameraHeight, cameraWidth, _partsDictPtr);
	_workPtrParts = malloc(_workSizeParts);

	_workSizeAllParts = sceFaceAllPartsGetWorkingMemorySize(cameraWidth, cameraHeight, cameraWidth, _allPartsDictPtr);
	_workPtrAllParts = malloc(_workSizeAllParts);

	_workAttribSize = sceFaceAttributeGetWorkingMemorySize(cameraWidth, cameraHeight, cameraWidth, _attribDictPtr);
	_workAttribPtr = malloc(_workAttribSize);

	_workSizeShape = sceFaceShapeGetWorkingMemorySize(cameraWidth, cameraHeight, cameraWidth, _shapeDictPtr, cameraWidth, cameraHeight, true);
	_workPtrShape = malloc(_workSizeShape);

	SceInt32 cameraBufSize;
	ScePVoid buf;
	cam->GetIBuffer(&buf, &cameraBufSize);

	_iBufferPrevious = (SceUInt8 *)malloc(cameraBufSize); //for tracking
}

LRFace::~LRFace()
{
	
}

SceInt32 LRFace::TrackThreadStart(SceSize args, ScePVoid argp)
{
	s_instance->TrackThread();

	return 0;
}

SceVoid LRFace::TrackThread()
{
	SceInt32 ret;
	SceInt32 face_ret = -1;
	SceInt32 parts_ret = -1;
	SceInt32 shape_ret = -1;
	SceInt32 attr_ret = -1;
	unsigned char *camBuffer;
	SceInt32 camWidth;
	SceInt32 camHeight;
	SceUInt64 frame;
	SceUInt64 timestamp;

	// Set resultPrecision to SCE_FACE_DETECT_RESULT_NORMAL for speed.
	// After global search face detection, it is always done local search
	// face detection, so that resultPrecision set to SCE_FACE_DETECT_RESULT_NORMAL.
	SceFaceDetectionParam detectParam;
	sceFaceDetectionGetDefaultParam(&detectParam);
	detectParam.resultPrecision = SCE_FACE_DETECT_RESULT_PRECISE;
	detectParam.searchType = SCE_FACE_DETECT_SEARCH_FACE_NUM_LIMIT;
	detectParam.magBegin = 0.5f;
	detectParam.magStep = 0.841f;
	detectParam.magEnd = 0.0f;
	detectParam.xScanStep = 2;
	detectParam.yScanStep = 2;
	detectParam.thresholdScore = 0.5f;

	SceFaceDetectionResult face[2];
	SceFaceAttribResult    attr[SCE_FACE_ATTRIB_NUM_MAX];

	while (1) {

		sceKernelLockLwMutex(&_faceMtx, 1, NULL);

		LRCamera::GetInstance()->Update(
			&camBuffer, &camWidth, &camHeight,
			&frame, &timestamp, SCE_TRUE
		);

		if (_prevFrame != frame) {
			_prevFrame = frame;

			// TODO: Render camera image here

			SceInt32 numFace;
			SceInt32 numAttrib;
			if (!_isTracking) {
				face_ret = sceFaceDetectionEx(
					camBuffer, camWidth, camHeight, camWidth,
					_detectDictPtr,
					&detectParam,
					&face[0], 1,
					&numFace,
					_workPtr, _workSize
				);

				if (face_ret == SCE_OK) {
					if (numFace > 0) {

						/*face_ret = sceFaceDetectionLocal(
							yuvBuffer, yuvWidth, yuvHeight, yuvWidth,
							_detectLocalDictPtr,
							0.841f, 1.3f, 1.3f, 1, 1, 0.50f,
							&face[1], 1, &face[0], 1,
							&numFace,
							_workPtrLocal, _workSizeLocal
						);*/

						parts_ret = sceFacePartsEx(
							camBuffer, camWidth, camHeight, camWidth,
							_partsDictPtr,
							_partsCheckDictPtr,
							1, 1,
							&face[0],
							_parts, SCE_FACE_PARTS_NUM_MAX,
							&_numParts,
							_workPtrParts, _workSizeParts
						);

						/*parts_ret = sceFaceAllParts(
							camBuffer, camWidth, camHeight, camWidth,
							_allPartsDictPtr,
							_shapeApDictPtr,
							1, 1,
							&face[0],
							_allParts, SCE_FACE_ALLPARTS_NUM_MAX,
							&_numAllParts,
							_workPtrAllParts, _workSizeAllParts
						);*/

						/*attr_ret = sceFaceAttribute(
							yuvBuffer, yuvWidth, yuvHeight, yuvWidth,
							_attribDictPtr,
							&face[0], parts, numParts,
							attr, SCE_FACE_ATTRIB_NUM_MAX, &numAttrib,
							_workAttribPtr, _workAttribSize
						);*/

						if (_isShapeTrack) {
								shape_ret = sceFaceShapeFit(
									camBuffer, camWidth, camHeight, camWidth,
									_shapeDictPtr,
									&_shapeData, SCE_FACE_SHAPE_SCORE_LOST_THRES_MIN,
									&face[0], _parts, _numParts,
									_workPtrShape, _workSizeShape
								);

								//s_score = s_shapeData.score;

								if (shape_ret == SCE_OK) {
									_isTracking = SCE_TRUE;
								}
						}
					}
				}
			}
			else { // _isTracking == SCE_TRUE
				shape_ret = sceFaceShapeTrack(
					camBuffer, _iBufferPrevious, camWidth, camHeight, camWidth,
					_shapeDictPtr,
					&_shapeData, _lostThres,
					_workPtrShape, _workSizeShape
				);

				//s_score = s_shapeData.score;

				if (shape_ret != SCE_OK) {
					_isTracking = SCE_FALSE;
				}
			}

			sceClibMemcpy(_iBufferPrevious, camBuffer, camWidth * camHeight);

			// TODO: End camera rendering here

			if (_isTracking) { // Full tracking

				/*sceClibPrintf("face pitch: %f\n", _shapeData.facePitch);
				sceClibPrintf("face roll: %f\n", _shapeData.faceRoll);
				sceClibPrintf("face yaw: %f\n", _shapeData.faceYaw);

				sceClibPrintf("\nface rect widht: %f\n", _shapeData.rectWidth);
				sceClibPrintf("face rect height: %f\n", _shapeData.rectHeight);

				sceClibPrintf("\nface rect centerx: %f\n", _shapeData.rectCenterX);
				sceClibPrintf("face rect centery: %f\n", _shapeData.rectCenterY);

				sceClibPrintf("\npoint num: %d\n", _shapeData.pointNum);*/

				// TODO: draw wireframe here
				//drawShape(rgbaBuffer, rgbaWidth, rgbaHeight, rgbaPitch * 4, &s_shapeData);
			}
			else { // No tracking: can only get face pitch, roll, yaw
				if (parts_ret == SCE_OK) {
					/*SceFacePose pose;
					SceFaceRegion region;
					ret = sceFaceEstimatePoseRegion(
						camWidth, camHeight,
						&face[0], _parts, _numParts,
						&pose, &region
					);*/

					if (ret == SCE_OK) {

						/*sceClibPrintf("face pitch: %f\n", pose.facePitch);
						sceClibPrintf("face roll: %f\n", pose.faceRoll);
						sceClibPrintf("face yaw: %f\n", pose.faceYaw);*/

						// TODO: draw wireframe here
						/*sampleFaceDrawPoseRegionResult(
							rgbaBuffer, rgbaWidth, rgbaHeight, rgbaPitch * 4,
							&pose, &region,
							D_CYAN
						);*/
					}
				}
			}
		}

		sceKernelUnlockLwMutex(&_faceMtx, 1);
		sceDisplayWaitVblankStartMulti(2);
	}
}

SceBool LRFace::Calibrate(SceUInt32 *progress)
{
	unsigned char *camBuffer;
	SceInt32 camWidth;
	SceInt32 camHeight;
	SceUInt64 frame;
	SceUInt64 timestamp;
	SceBool result = SCE_FALSE;

	LRCamera *cam = LRCamera::GetInstance();

	sceKernelLockLwMutex(&_faceMtx, 1, NULL);

	cam->Update(
		&camBuffer, &camWidth, &camHeight,
		&frame, &timestamp, SCE_TRUE
	);

	if (_prevFrame != frame) {
		_prevFrame = frame;

		if (_waitFrameCount < _waitFrameNum) {
			/*E wait until the effect of sceCameraSetEV(). */
			_waitFrameCount++;
		}
		else {
			SceFaceDetectionResult face;
			SceInt32 numFace = 0;
			sceFaceDetection(
				camBuffer, camWidth, camHeight, camWidth,
				_detectDictPtr,
				0.5f, 0.841f, 0.0f, 2, 2, 0.80f, SCE_FACE_DETECT_RESULT_NORMAL,
				&face, 1,
				&numFace,
				_workPtr, _workSize
			);

			if (numFace == 0) {
				sceKernelUnlockLwMutex(&_faceMtx, 1);
				if (progress) {
					*progress = (SceUInt32)(((SceFloat)_evCalibrationNum / (SceFloat)_evLevelNum) * 100.0f);
				}
				return result;
			}

			_evScore[_evCalibrationNum] = face.score;
			sceClibPrintf("EV_Level:Score = %d:%f\n", _evLevelTable[_evCalibrationNum], _evScore[_evCalibrationNum]);
			_evCalibrationNum++;

			if (_evCalibrationNum < _evLevelNum) {
				_waitFrameCount = 0;
				cam->SetEv(_evLevelTable[_evCalibrationNum]);
			}
			else { // calibration finished
				SceInt32 maxLevelNum = -1;
				SceFloat maxScore = 0.f;

				for (int i = 0; i < _evLevelNum; i++) {
					if (maxScore < _evScore[i]) {
						maxScore = _evScore[i];
						maxLevelNum = i;
					}
				}

				if (maxLevelNum == -1) {
					sceClibPrintf("Calibration Error\n");
					cam->SetEv(_evLevel);
				}
				else {
					sceClibPrintf("max_level:max_score = %d:%f\n", _evLevelTable[maxLevelNum], maxScore);
					_evLevel = _evLevelTable[maxLevelNum];
					cam->SetEv(_evLevelTable[maxLevelNum]);
				}
				_evCalibrationNum = 0;
				result = SCE_TRUE;
			}
		}
	}

	sceKernelUnlockLwMutex(&_faceMtx, 1);

	if (progress) {
		if (result)
			*progress = 100;
		else
			*progress = (SceUInt32)(((SceFloat)_evCalibrationNum / (SceFloat)_evLevelNum) * 100.0f);
	}

	return result;
}

SceBool LRFace::GetTrackingState()
{
	return _isTracking;
}

SceVoid LRFace::GetBasicTrackingAngles(SceFloat *x, SceFloat *y)
{
	SceFloat rx = _shapeData.faceYaw * 2.0f;
	SceFloat ry = _shapeData.facePitch * -2.5f;

	if (isnan(rx))
		rx = 0.0f;
	if (isnan(ry))
		ry = 0.0f;

	*x = rx;
	*y = ry;
}

SceVoid LRFace::GetMouth(SceFloat *p1)
{
	SceFloat ret = (_shapeData.pointY[43] - _shapeData.pointY[40]) * 10.0f;
	if (isnan(ret))
		ret = 0.0f;
	*p1 = ret;
}

SceVoid LRFace::GetBrows(SceFloat *l, SceFloat *r)
{
	SceFloat rl = (_shapeData.pointY[14] - _shapeData.pointY[22]);
	SceFloat rr = (_shapeData.pointY[1] - _shapeData.pointY[18]);

	if (isnan(rl))
		rl = 0.0f;
	if (isnan(rr))
		rr = 0.0f;

	*l = rl;
	*r = rr;
}

int idx = 0;

SceVoid LRFace::DrawShape()
{
	if (_isTracking) {
		for (int i = 0; i < _shapeData.pointNum; i++) {

			SceFloat sx = (int)(160 * _shapeData.pointX[i]);
			SceFloat dx = (int)(160 * _shapeData.pointX[_shapeConnectTo[_shapeData.modelID][i]]);
			SceFloat sy = (int)(120 * _shapeData.pointY[i]);
			SceFloat dy = (int)(120 * _shapeData.pointY[_shapeConnectTo[_shapeData.modelID][i]]);

			//if (i == idx)
			vita2d_draw_line(sx, sy, dx, dy, RGBA8(255, 0, 0, 255));
		}
	}

	char val[1];

	if (sceTargetTransportTryRecv(val, sizeof(val)) == sizeof(val)) {
		if (val[0] == ',') {
			idx++;
			sceClibPrintf("idx: %d\n", idx);
		}
		else if (val[0] == 'S') {
			idx--;
			sceClibPrintf("idx: %d\n", idx);
		}
	}
}

SceVoid LRFace::StartTracking()
{
	SceUID updateThread = sceKernelCreateThread("LRFace:UpdateThread", TrackThreadStart, 64, 0x100000, 0, SCE_KERNEL_CPU_MASK_USER_1, NULL);
	sceKernelStartThread(updateThread, 0, NULL);
}
