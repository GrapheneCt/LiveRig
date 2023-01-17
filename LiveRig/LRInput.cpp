#include <kernel.h>
#include <libdbg.h>
#include <libsysmodule.h>
#include <ctrl.h>
#include <scetypes.h>

#include "LRInput.hpp"

namespace {
	LRInput *s_instance = SCE_NULL;
}

LRInput::LRInput()
{
	s_instance = this;
}

LRInput::~LRInput()
{
	
}

LRInput *LRInput::GetInstance()
{
	if (s_instance == SCE_NULL)
	{
		s_instance = new LRInput();
	}

	return s_instance;
}

SceVoid LRInput::ReleaseInstance()
{
	if (s_instance != SCE_NULL)
	{
		delete s_instance;
	}

	s_instance = SCE_NULL;
}

SceVoid LRInput::UpdateBegin()
{
	sceCtrlPeekBufferPositive(0, &_ctrl, 1);
}

SceVoid LRInput::UpdateEnd()
{
	sceClibMemcpy(&_ctrlOld, &_ctrl, sizeof(SceCtrlData));
}

SceBool LRInput::CheckPressedState(SceInt32 button)
{
	if ((_ctrl.buttons & (button)) && !(_ctrlOld.buttons & (button)))
		return SCE_TRUE;
	else
		return SCE_FALSE;
}

SceBool LRInput::CheckHoldState(SceInt32 button)
{
	if (_ctrl.buttons & button)
		return SCE_TRUE;
	else
		return SCE_FALSE;
}