#pragma once

#include <kernel.h>
#include <ctrl.h>
#include <scetypes.h>

class LRInput
{
public:

	static LRInput *GetInstance();

	static SceVoid ReleaseInstance();

	SceVoid UpdateBegin();

	SceVoid UpdateEnd();

	SceBool CheckPressedState(SceInt32 button);

	SceBool CheckHoldState(SceInt32 button);

private:

	SceCtrlData _ctrl;
	SceCtrlData _ctrlOld;

	LRInput();

	~LRInput();
};

