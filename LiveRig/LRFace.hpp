#pragma once

#include <kernel.h>
#include <camera.h>
#include <libface.h>
#include <scetypes.h>

class LRFace
{
public:

	static LRFace *GetInstance();

	static SceVoid ReleaseInstance();

	SceVoid StartTracking();

	SceBool Calibrate(SceUInt32 *progress);

	SceVoid DrawShape();

	SceBool GetTrackingState();

	SceVoid GetBasicTrackingAngles(SceFloat *x, SceFloat *y);

	SceVoid GetMouth(SceFloat *p1);

	SceVoid GetBrows(SceFloat *l, SceFloat *r);

private:

	const SceInt32 _waitFrameNum = 30;
	const SceInt32 _evLevelNum = 17;

	SceUInt8 *_detectDictPtr;
	SceUInt8 *_detectLocalDictPtr;
	SceUInt8 *_partsDictPtr;
	SceUInt8 *_allPartsDictPtr;
	SceFaceAttribDictPtr _attribDictPtr;
	SceUInt8 *_partsCheckDictPtr;
	SceFaceShapeModelDictPtr _shapeDictPtr;
	SceFaceShapeDictPtr _shapeApDictPtr;

	SceInt32 _workSize;
	ScePVoid _workPtr;

	SceInt32 _workSizeLocal;
	ScePVoid _workPtrLocal;

	SceInt32 _workSizeParts;
	ScePVoid _workPtrParts;

	SceInt32 _workSizeAllParts;
	ScePVoid _workPtrAllParts;

	SceInt32 _workAttribSize;
	ScePVoid _workAttribPtr;

	SceInt32 _workSizeShape;
	ScePVoid _workPtrShape;

	SceUInt8 *_iBufferPrevious;

	SceUInt64 _prevFrame;

	SceBool _isTracking;
	SceBool _isShapeTrack;

	SceFaceShapeResult _shapeData;

	SceFloat _lostThres;

	SceInt32 _evCalibrationNum;
	SceInt32 _waitFrameCount;
	
	SceFloat _evScore[17];
	SceInt32 _evLevel;

	SceInt32 _numParts;
	SceFacePartsResult _parts[SCE_FACE_PARTS_NUM_MAX];

	SceInt32 _numAllParts;
	SceFacePartsResult _allParts[SCE_FACE_ALLPARTS_NUM_MAX];

	const SceInt32 _evLevelTable[17] = {
		SCE_CAMERA_ATTRIBUTE_EV_MINUS_2,	// -20
		SCE_CAMERA_ATTRIBUTE_EV_MINUS_1_7,	// -17
		SCE_CAMERA_ATTRIBUTE_EV_MINUS_1_5,	// -15
		SCE_CAMERA_ATTRIBUTE_EV_MINUS_1_3,	// -13
		SCE_CAMERA_ATTRIBUTE_EV_MINUS_1,	// -10
		SCE_CAMERA_ATTRIBUTE_EV_MINUS_0_7,	// - 7
		SCE_CAMERA_ATTRIBUTE_EV_MINUS_0_5,	// - 5
		SCE_CAMERA_ATTRIBUTE_EV_MINUS_0_3,	// - 3
		SCE_CAMERA_ATTRIBUTE_EV_0,			//   0
		SCE_CAMERA_ATTRIBUTE_EV_PLUS_0_3,	//   3
		SCE_CAMERA_ATTRIBUTE_EV_PLUS_0_5,	//   5
		SCE_CAMERA_ATTRIBUTE_EV_PLUS_0_7,	//   7
		SCE_CAMERA_ATTRIBUTE_EV_PLUS_1,		//  10
		SCE_CAMERA_ATTRIBUTE_EV_PLUS_1_3,	//  13
		SCE_CAMERA_ATTRIBUTE_EV_PLUS_1_5,	//  15
		SCE_CAMERA_ATTRIBUTE_EV_PLUS_1_7,	//  17
		SCE_CAMERA_ATTRIBUTE_EV_PLUS_2		//  20
	};

	const unsigned char _shapeConnectTo[SCE_FACE_SHAPE_MODEL_NUM_MAX][SCE_FACE_SHAPE_POINT_NUM_MAX] = {
		//SCE_FACE_SHAPE_MODEL_ID_FRONTAL (0)
		{1, 2, 3, 4, 5, 6, 7, 0, 9, 10, 11, 12, 13, 14, 15, 8, 17, 18, 19, 20, 20, 22, 23, 24, 25, 25, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 36, 38, 39, 40, 41, 42, 43, 44, 37, 45},
	};

	SceKernelLwMutexWork _faceMtx;

	LRFace();

	~LRFace();

	static SceInt32 TrackThreadStart(SceSize args, ScePVoid argp);

	SceVoid TrackThread();
};

