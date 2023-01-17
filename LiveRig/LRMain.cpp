#include <CubismFramework.hpp>
#include <kernel.h>
#include <ctrl.h>
#include <libdbg.h>
#include <message_dialog.h>
#include <vita2d_sys.h>

#pragma comment(lib, "libSceTargetTransport_stub.a")
#include <target_transport.h>

#include "LRGXM.hpp"
#include "LRCamera.hpp"
#include "LRFace.hpp"
#include "LRInput.hpp"
#include "LRCubismAllocator.hpp"
#include "LRAppLevel.hpp"

using namespace Csm;

SCE_USER_MODULE_LIST("app0:module/libvita2d_sys.suprx");

// User main thread parameters
const char		sceUserMainThreadName[] = "simple_main_thr";
int				sceUserMainThreadPriority = SCE_KERNEL_DEFAULT_PRIORITY_USER;
unsigned int	sceUserMainThreadCpuAffinityMask = SCE_KERNEL_CPU_MASK_USER_0;
unsigned int	sceUserMainThreadStackSize = SCE_KERNEL_STACK_SIZE_DEFAULT_USER_MAIN;

// Libc parameters
unsigned int	sceLibcHeapSize = 64 * 1024 * 1024;

static LRCubismAllocator cubismAllocator;
static Csm::CubismFramework::Option cubismOption;

static vita2d_pvf *font;

int showDialog(int mode, int type, bool infobar, bool dimmer, const char *str)
{
	SceMsgDialogParam				msgParam;
	SceMsgDialogUserMessageParam	userMsgParam;
	SceMsgDialogProgressBarParam	progBarParam;

	SceCommonDialogInfobarParam	infobarParam;
	SceCommonDialogColor		dimmerColor;

	// initalize parameter of message dialog
	sceMsgDialogParamInit(&msgParam);
	msgParam.mode = mode;

	// initalize message dialog
	if (mode == SCE_MSG_DIALOG_MODE_USER_MSG) {
		sceClibMemset(&userMsgParam, 0, sizeof(SceMsgDialogUserMessageParam));
		msgParam.userMsgParam = &userMsgParam;
		msgParam.userMsgParam->msg = (const SceChar8 *)str;
		msgParam.userMsgParam->buttonType = type;
	}
	else if (mode == SCE_MSG_DIALOG_MODE_PROGRESS_BAR) {
		sceClibMemset(&progBarParam, 0, sizeof(SceMsgDialogProgressBarParam));
		msgParam.progBarParam = &progBarParam;
		msgParam.progBarParam->barType = SCE_MSG_DIALOG_PROGRESSBAR_TYPE_PERCENTAGE;
		msgParam.progBarParam->msg = (const SceChar8 *)str;
	}

	// initalize parameters for infobar
	if (infobar) {
		sceClibMemset(&infobarParam, 0, sizeof(infobarParam));
		infobarParam.visibility = true;
		infobarParam.color = 0;
		infobarParam.transparency = 0;
		msgParam.commonParam.infobarParam = &infobarParam;
	}
	else
		msgParam.commonParam.infobarParam = NULL;

	// initalize parameters for dimmer color
	if (dimmer) {
		dimmerColor.r = 0;
		dimmerColor.g = 0;
		dimmerColor.b = 0;
		dimmerColor.a = 255;
		msgParam.commonParam.dimmerColor = &dimmerColor;
	}
	else
		msgParam.commonParam.dimmerColor = NULL;

	msgParam.commonParam.bgColor = NULL;

	sceMsgDialogTerm();

	return sceMsgDialogInit(&msgParam);
}

int showMessageDialog(int type, bool infobar, bool dimmer, const char *str)
{
	return showDialog(SCE_MSG_DIALOG_MODE_USER_MSG, type, infobar, dimmer, str);
}

int showProgressDialog(bool infobar, bool dimmer, const char *str)
{
	return showDialog(SCE_MSG_DIALOG_MODE_PROGRESS_BAR, 0, infobar, dimmer, str);
}

int updateProgressDialog(SceUInt32 value)
{
	return sceMsgDialogProgressBarSetValue(SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT, value);
}

void initializeCubism()
{
	//setup cubism
	cubismOption.LogFunction = (Live2D::Cubism::Core::csmLogFunction)sceClibPrintf;
	cubismOption.LoggingLevel = CubismFramework::Option::LogLevel::LogLevel_Verbose;
	CubismFramework::StartUp(&cubismAllocator, &cubismOption);

	//Initialize cubism
	CubismFramework::Initialize();

	LRAppLevel::UpdateTime();
}

int main()
{
	sceKernelLoadStartModule("ur0:data/external/libTargetTransport.suprx", 0, NULL, 0, NULL, NULL);
	sceTargetTransportConnect();

	SceBool isCalibrating = SCE_FALSE;

	sceDbgSetMinimumLogLevel(SCE_DBG_LOG_LEVEL_TRACE);

	LRGXM *render = LRGXM::GetInstance();
	LRInput *input = LRInput::GetInstance();

	LRGXM::InitParams params;
	sceClibMemset(&params, 0, sizeof(LRGXM::InitParams));

	render->Init(&params);

	vita2d_system_pvf_config configs[] = {
		{SCE_PVF_LANGUAGE_LATIN, SCE_PVF_FAMILY_SANSERIF, SCE_PVF_STYLE_REGULAR, NULL},
	};

	font = vita2d_load_system_pvf(1, configs, 13, 13);

	LRAppLevel *app = LRAppLevel::GetInstance();
	app->Initialize();

	initializeCubism();

	app->LoadModel("app0:Resources/Hiyori/", "Hiyori");

	LRCamera *cam = LRCamera::GetInstance();
	LRFace *face = LRFace::GetInstance();

	cam->Start(SCE_CAMERA_DEVICE_FRONT);
	face->StartTracking();

	SceFloat xAngle, yAngle, mouthPoint, browLY, browRY;

	while (1) {

		LRAppLevel::UpdateTime();

		vita2d_pool_reset();
		render->StartScene();
		vita2d_clear_screen();
		input->UpdateBegin();

		if (input->CheckPressedState(SCE_CTRL_CROSS)) {
			isCalibrating = SCE_TRUE;
			showProgressDialog(false, false, "Camera calibration is in progress. Please hold still and don't move your PS Vita system unless there is no progress for a long time.");
		}

		if (isCalibrating) {
			SceUInt32 progress = 0;
			isCalibrating = !face->Calibrate(&progress);
			updateProgressDialog(progress);
			if (!isCalibrating) {
				sceMsgDialogClose();
			}
		}

		cam->DrawCamTex();
		face->DrawShape();

		if (face->GetTrackingState())
			vita2d_pvf_draw_text(font, 20, 220, RGBA8(0, 255, 0, 255), 1.0f, "Tracking: OK");
		else
			vita2d_pvf_draw_text(font, 20, 220, RGBA8(255, 0, 0, 255), 1.0f, "Tracking: face lost");

		face->GetBasicTrackingAngles(&xAngle, &yAngle);
		face->GetMouth(&mouthPoint);
		face->GetBrows(&browLY, &browRY);
		vita2d_pvf_draw_textf(font, 20, 250, RGBA8(0, 0, 0, 255), 1.0f, "Mouth: %.4f", mouthPoint);
		vita2d_pvf_draw_textf(font, 20, 280, RGBA8(0, 0, 0, 255), 1.0f, "Face x: %.4f", xAngle);
		vita2d_pvf_draw_textf(font, 20, 310, RGBA8(0, 0, 0, 255), 1.0f, "Face y: %.4f", yAngle);
		vita2d_pvf_draw_textf(font, 20, 340, RGBA8(0, 0, 0, 255), 1.0f, "Left brow: %.4f", browLY);
		vita2d_pvf_draw_textf(font, 20, 370, RGBA8(0, 0, 0, 255), 1.0f, "Right brow: %.4f", browRY);

		render->EndScene();

		app->RenderModel(xAngle, yAngle, mouthPoint, browLY, browRY);

		render->UpdateCommonDialog();
		render->EndRendering();
		input->UpdateEnd();
	}

	return 0;
}