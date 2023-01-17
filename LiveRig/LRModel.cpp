#include <fstream>
#include <vector>
#include <kernel.h>
#include <libdbg.h>
#include <vita2d_sys.h>
#include <CubismModelSettingJson.hpp>
#include <Motion/CubismMotion.hpp>
#include <Physics/CubismPhysics.hpp>
#include <CubismDefaultParameterId.hpp>
#include <Rendering/GXM/CubismRenderer_GXM.hpp>
#include <Utils/CubismString.hpp>
#include <Id/CubismIdManager.hpp>
#include <Motion/CubismMotionQueueEntry.hpp>

#include "LRModel.hpp"
#include "LRAppLevel.hpp"
#include "LRGXM.hpp"

using namespace Live2D::Cubism::Framework;
using namespace Live2D::Cubism::Framework::DefaultParameterId;

namespace {
	csmByte* CreateBuffer(const csmChar* path, csmSizeInt* size)
	{
		SceIoStat stat;
		SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
		sceIoGetstatByFd(fd, &stat);
		*size = stat.st_size;
		csmByte *buf = (csmByte *)malloc(*size);
		sceIoRead(fd, buf, *size);
		sceIoClose(fd);
		return buf;
	}

	void DeleteBuffer(csmByte* buffer, const csmChar* path = "")
	{
		free((void *)buffer);
	}
}

LRModel::LRModel()
	: CubismUserModel()
	, _modelSetting(NULL)
	, _userTimeSeconds(0.0f)
	, _vitaProjectionFactor(1.0f)
{
	_debugMode = true;

	_idParamAngleX = CubismFramework::GetIdManager()->GetId(ParamAngleX);
	_idParamAngleY = CubismFramework::GetIdManager()->GetId(ParamAngleY);
	_idParamAngleZ = CubismFramework::GetIdManager()->GetId(ParamAngleZ);
	_idParamBodyAngleX = CubismFramework::GetIdManager()->GetId(ParamBodyAngleX);
	_idParamEyeBallX = CubismFramework::GetIdManager()->GetId(ParamEyeBallX);
	_idParamEyeBallY = CubismFramework::GetIdManager()->GetId(ParamEyeBallY);
	_idParamBrowLY = CubismFramework::GetIdManager()->GetId(ParamBrowLY);
	_idParamBrowRY = CubismFramework::GetIdManager()->GetId(ParamBrowRY);
	_idParamMouthOpenY = CubismFramework::GetIdManager()->GetId(ParamMouthOpenY);
}

LRModel::~LRModel()
{
	LRGXM::GetInstance()->WaitRenderingDone();

	_renderBuffer.DestroyOffscreenFrame();

	ReleaseMotions();
	ReleaseExpressions();

	for (csmInt32 i = 0; i < _modelSetting->GetMotionGroupCount(); i++)
	{
		const csmChar* group = _modelSetting->GetMotionGroupName(i);
		ReleaseMotionGroup(group);
	}
	delete(_modelSetting);
}

void LRModel::LoadAssets(const csmChar* dir, const csmChar* fileName)
{
	_modelHomeDir = dir;

	if (_debugMode)
	{
		SCE_DBG_LOG_DEBUG("[APP]load model setting: %s", fileName);
	}

	csmSizeInt size;
	const csmString path = csmString(dir) + fileName;

	csmByte* buffer = CreateBuffer(path.GetRawString(), &size);
	ICubismModelSetting* setting = new CubismModelSettingJson(buffer, size);
	DeleteBuffer(buffer, path.GetRawString());

	SetupModel(setting);

	LRGXM* gxmC = LRGXM::GetInstance();
	Rendering::CubismRenderer_GXM::GXMContext context;

	context.immContext = &gxmC->immContext;
	context.msaa = &gxmC->msaa;
	context.renderTarget = &gxmC->renderTarget;
	context.shaderPatcher = &gxmC->shaderPatcher;
	context.displayColorSurface[0] = &gxmC->displayColorSurface[0];
	context.displayColorSurface[1] = &gxmC->displayColorSurface[1];
	context.displayBufferSync[0] = &gxmC->displayBufferSync[0];
	context.displayBufferSync[1] = &gxmC->displayBufferSync[1];
	context.displayBufferData[0] = &gxmC->displayBufferData[0];
	context.displayBufferData[1] = &gxmC->displayBufferData[1];
	context.depthStencilSurface = &gxmC->depthStencilSurface;
	context.displayWidth = (uint32_t *)&displayWidth;
	context.displayHeight = (uint32_t *)&displayHeight;

	CreateRenderer((void *)&context);

	SetupTextures();
}


void LRModel::SetupModel(ICubismModelSetting* setting)
{
	_updating = true;
	_initialized = false;

	_modelSetting = setting;

	csmByte* buffer;
	csmSizeInt size;

	//Cubism Model
	if (strcmp(_modelSetting->GetModelFileName(), "") != 0)
	{
		csmString path = _modelSetting->GetModelFileName();
		path = _modelHomeDir + path;

		SCE_DBG_LOG_DEBUG("[APP]create model: %s", setting->GetModelFileName());

		buffer = CreateBuffer(path.GetRawString(), &size);
		LoadModel(buffer, size);
		DeleteBuffer(buffer, path.GetRawString());
	}

	//Expression
	if (_modelSetting->GetExpressionCount() > 0)
	{
		const csmInt32 count = _modelSetting->GetExpressionCount();
		for (csmInt32 i = 0; i < count; i++)
		{
			csmString name = _modelSetting->GetExpressionName(i);
			csmString path = _modelSetting->GetExpressionFileName(i);
			path = _modelHomeDir + path;

			buffer = CreateBuffer(path.GetRawString(), &size);
			ACubismMotion* motion = LoadExpression(buffer, size, name.GetRawString());

			if (_expressions[name] != NULL)
			{
				ACubismMotion::Delete(_expressions[name]);
				_expressions[name] = NULL;
			}
			_expressions[name] = motion;

			DeleteBuffer(buffer, path.GetRawString());
		}
	}

	//Physics
	if (strcmp(_modelSetting->GetPhysicsFileName(), "") != 0)
	{
		csmString path = _modelSetting->GetPhysicsFileName();
		path = _modelHomeDir + path;

		buffer = CreateBuffer(path.GetRawString(), &size);
		LoadPhysics(buffer, size);
		DeleteBuffer(buffer, path.GetRawString());
	}

	//Pose
	if (strcmp(_modelSetting->GetPoseFileName(), "") != 0)
	{
		csmString path = _modelSetting->GetPoseFileName();
		path = _modelHomeDir + path;

		buffer = CreateBuffer(path.GetRawString(), &size);
		LoadPose(buffer, size);
		DeleteBuffer(buffer, path.GetRawString());
	}

	//EyeBlink
	if (_modelSetting->GetEyeBlinkParameterCount() > 0)
	{
		_eyeBlink = CubismEyeBlink::Create(_modelSetting);
	}

	//Breath
	{
		_breath = CubismBreath::Create();

		csmVector<CubismBreath::BreathParameterData> breathParameters;

		breathParameters.PushBack(CubismBreath::BreathParameterData(_idParamAngleX, 0.0f, 15.0f, 6.5345f, 0.5f));
		breathParameters.PushBack(CubismBreath::BreathParameterData(_idParamAngleY, 0.0f, 8.0f, 3.5345f, 0.5f));
		breathParameters.PushBack(CubismBreath::BreathParameterData(_idParamAngleZ, 0.0f, 10.0f, 5.5345f, 0.5f));
		breathParameters.PushBack(CubismBreath::BreathParameterData(_idParamBodyAngleX, 0.0f, 4.0f, 15.5345f, 0.5f));
		breathParameters.PushBack(CubismBreath::BreathParameterData(CubismFramework::GetIdManager()->GetId(ParamBreath), 0.5f, 0.5f, 3.2345f, 0.5f));

		_breath->SetParameters(breathParameters);
	}

	//UserData
	if (strcmp(_modelSetting->GetUserDataFile(), "") != 0)
	{
		csmString path = _modelSetting->GetUserDataFile();
		path = _modelHomeDir + path;
		buffer = CreateBuffer(path.GetRawString(), &size);
		LoadUserData(buffer, size);
		DeleteBuffer(buffer, path.GetRawString());
	}

	// EyeBlinkIds
	{
		csmInt32 eyeBlinkIdCount = _modelSetting->GetEyeBlinkParameterCount();
		for (csmInt32 i = 0; i < eyeBlinkIdCount; ++i)
		{
			_eyeBlinkIds.PushBack(_modelSetting->GetEyeBlinkParameterId(i));
		}
	}

	// LipSyncIds
	{
		csmInt32 lipSyncIdCount = _modelSetting->GetLipSyncParameterCount();
		for (csmInt32 i = 0; i < lipSyncIdCount; ++i)
		{
			_lipSyncIds.PushBack(_modelSetting->GetLipSyncParameterId(i));
		}
	}

	//Layout
	csmMap<csmString, csmFloat32> layout;
	_modelSetting->GetLayoutMap(layout);
	_modelMatrix->SetupFromLayout(layout);

	_model->SaveParameters();

	for (csmInt32 i = 0; i < _modelSetting->GetMotionGroupCount(); i++)
	{
		const csmChar* group = _modelSetting->GetMotionGroupName(i);
		PreloadMotionGroup(group);
	}

	_motionManager->StopAllMotions();

	_updating = false;
	_initialized = true;
}

void LRModel::PreloadMotionGroup(const csmChar* group)
{
	const csmInt32 count = _modelSetting->GetMotionCount(group);

	for (csmInt32 i = 0; i < count; i++)
	{
		//ex) idle_0
		csmString name = Utils::CubismString::GetFormatedString("%s_%d", group, i);
		csmString path = _modelSetting->GetMotionFileName(group, i);
		path = _modelHomeDir + path;

		SCE_DBG_LOG_DEBUG("[APP]load motion: %s => [%s_%d] ", path.GetRawString(), group, i);

		csmByte* buffer;
		csmSizeInt size;
		buffer = CreateBuffer(path.GetRawString(), &size);
		CubismMotion* tmpMotion = static_cast<CubismMotion*>(LoadMotion(buffer, size, name.GetRawString()));

		csmFloat32 fadeTime = _modelSetting->GetMotionFadeInTimeValue(group, i);
		if (fadeTime >= 0.0f)
		{
			tmpMotion->SetFadeInTime(fadeTime);
		}

		fadeTime = _modelSetting->GetMotionFadeOutTimeValue(group, i);
		if (fadeTime >= 0.0f)
		{
			tmpMotion->SetFadeOutTime(fadeTime);
		}
		tmpMotion->SetEffectIds(_eyeBlinkIds, _lipSyncIds);

		if (_motions[name] != NULL)
		{
			ACubismMotion::Delete(_motions[name]);
		}
		_motions[name] = tmpMotion;

		DeleteBuffer(buffer, path.GetRawString());
	}
}

void LRModel::ReleaseMotionGroup(const csmChar* group) const
{
	const csmInt32 count = _modelSetting->GetMotionCount(group);
	for (csmInt32 i = 0; i < count; i++)
	{
		csmString voice = _modelSetting->GetMotionSoundFileName(group, i);
		if (strcmp(voice.GetRawString(), "") != 0)
		{
			csmString path = voice;
			path = _modelHomeDir + path;
		}
	}
}

void LRModel::ReleaseMotions()
{
	for (csmMap<csmString, ACubismMotion*>::const_iterator iter = _motions.Begin(); iter != _motions.End(); ++iter)
	{
		ACubismMotion::Delete(iter->Second);
	}

	_motions.Clear();
}

void LRModel::ReleaseExpressions()
{
	for (csmMap<csmString, ACubismMotion*>::const_iterator iter = _expressions.Begin(); iter != _expressions.End(); ++iter)
	{
		ACubismMotion::Delete(iter->Second);
	}

	_expressions.Clear();
}

void LRModel::Update(csmFloat32 xAngle, csmFloat32 yAngle, csmFloat32 mouth, csmFloat32 browLY, csmFloat32 browRY)
{
	const csmFloat32 deltaTimeSeconds = LRAppLevel::GetDeltaTime();
	_userTimeSeconds += deltaTimeSeconds;

	_dragManager->Set(xAngle, yAngle);

	_dragManager->Update(deltaTimeSeconds);
	_dragX = _dragManager->GetX();
	_dragY = _dragManager->GetY();

	csmBool motionUpdated = false;

	//-----------------------------------------------------------------
	/*_model->LoadParameters();
	if (_motionManager->IsFinished())
	{
		StartRandomMotion(MotionGroupIdle, PriorityIdle);
	}
	else
	{
		motionUpdated = _motionManager->UpdateMotion(_model, deltaTimeSeconds);
	}
	_model->SaveParameters();*/
	//-----------------------------------------------------------------

	if (!motionUpdated)
	{
		if (_eyeBlink != NULL)
		{
			_eyeBlink->UpdateParameters(_model, deltaTimeSeconds);
		}
	}

	if (_expressionManager != NULL)
	{
		_expressionManager->UpdateMotion(_model, deltaTimeSeconds);
	}


	_model->SetParameterValue(_idParamAngleX, _dragX * 30);
	_model->SetParameterValue(_idParamAngleY, _dragY * 30);
	_model->SetParameterValue(_idParamAngleZ, _dragX * _dragY * -30);

	_model->SetParameterValue(_idParamBodyAngleX, _dragX * 10);

	/*
	_model->SetParameterValue(_idParamEyeBallX, _dragX);
	_model->SetParameterValue(_idParamEyeBallY, _dragY);
	*/

	_model->SetParameterValue(_idParamBrowLY, browLY);
	_model->SetParameterValue(_idParamBrowRY, browRY);


	if (_breath != NULL)
	{
		//_breath->UpdateParameters(_model, deltaTimeSeconds);
	}


	if (_physics != NULL)
	{
		_physics->Evaluate(_model, deltaTimeSeconds);
	}


	if (_lipSync)
	{
		/*
		for (csmUint32 i = 0; i < _lipSyncIds.GetSize(); ++i)
		{
			_model->SetParameterValue(_lipSyncIds[i], mouth);
		}
		*/

		_model->SetParameterValue(_idParamMouthOpenY, mouth);
	}

	if (_pose != NULL)
	{
		_pose->UpdateParameters(_model, deltaTimeSeconds);
	}

	_model->Update();

}

CubismMotionQueueEntryHandle LRModel::StartMotion(const csmChar* group, csmInt32 no, csmInt32 priority, ACubismMotion::FinishedMotionCallback onFinishedMotionHandler)
{
	if (priority == PriorityForce)
	{
		_motionManager->SetReservePriority(priority);
	}
	else if (!_motionManager->ReserveMotion(priority))
	{

		SCE_DBG_LOG_DEBUG("[APP]can't start motion.");

		return InvalidMotionQueueEntryHandleValue;
	}

	const csmString fileName = _modelSetting->GetMotionFileName(group, no);

	//ex) idle_0
	csmString name = Utils::CubismString::GetFormatedString("%s_%d", group, no);
	CubismMotion* motion = static_cast<CubismMotion*>(_motions[name.GetRawString()]);
	csmBool autoDelete = false;

	if (motion == NULL)
	{
		csmString path = fileName;
		path = _modelHomeDir + path;

		csmByte* buffer;
		csmSizeInt size;
		buffer = CreateBuffer(path.GetRawString(), &size);
		motion = static_cast<CubismMotion*>(LoadMotion(buffer, size, NULL, onFinishedMotionHandler));
		csmFloat32 fadeTime = _modelSetting->GetMotionFadeInTimeValue(group, no);
		if (fadeTime >= 0.0f)
		{
			motion->SetFadeInTime(fadeTime);
		}

		fadeTime = _modelSetting->GetMotionFadeOutTimeValue(group, no);
		if (fadeTime >= 0.0f)
		{
			motion->SetFadeOutTime(fadeTime);
		}
		motion->SetEffectIds(_eyeBlinkIds, _lipSyncIds);
		autoDelete = true;

		DeleteBuffer(buffer, path.GetRawString());
	}
	else
	{
		motion->SetFinishedMotionHandler(onFinishedMotionHandler);
	}

	csmString voice = _modelSetting->GetMotionSoundFileName(group, no);
	if (strcmp(voice.GetRawString(), "") != 0)
	{
		csmString path = voice;
		path = _modelHomeDir + path;
	}

	SCE_DBG_LOG_DEBUG("[APP]start motion: [%s_%d]", group, no);

	return  _motionManager->StartMotionPriority(motion, autoDelete, priority);
}

CubismMotionQueueEntryHandle LRModel::StartRandomMotion(const csmChar* group, csmInt32 priority, ACubismMotion::FinishedMotionCallback onFinishedMotionHandler)
{
	if (_modelSetting->GetMotionCount(group) == 0)
	{
		return InvalidMotionQueueEntryHandleValue;
	}

	csmInt32 no = rand() % _modelSetting->GetMotionCount(group);

	return StartMotion(group, no, priority, onFinishedMotionHandler);
}

void LRModel::DoDraw()
{
	if (_model == NULL)
	{
		return;
	}

	GetRenderer<Rendering::CubismRenderer_GXM>()->SetDisplayIndex(LRGXM::GetInstance()->GetDisplayIndex());
	GetRenderer<Rendering::CubismRenderer_GXM>()->DrawModel();
}

void LRModel::Draw(CubismMatrix44& matrix)
{
	if (_model == NULL)
	{
		return;
	}

	matrix.MultiplyByMatrix(_modelMatrix);

	GetRenderer<Rendering::CubismRenderer_GXM>()->SetMvpMatrix(&matrix);

	DoDraw();
}

csmBool LRModel::HitTest(const csmChar* hitAreaName, csmFloat32 x, csmFloat32 y)
{
	if (_opacity < 1)
	{
		return false;
	}
	const csmInt32 count = _modelSetting->GetHitAreasCount();
	for (csmInt32 i = 0; i < count; i++)
	{
		if (strcmp(_modelSetting->GetHitAreaName(i), hitAreaName) == 0)
		{
			const CubismIdHandle drawID = _modelSetting->GetHitAreaId(i);
			return IsHit(drawID, x, y);
		}
	}
	return false;
}

void LRModel::SetExpression(const csmChar* expressionID)
{
	ACubismMotion* motion = _expressions[expressionID];

	SCE_DBG_LOG_DEBUG("[APP]expression: [%s]", expressionID);

	if (motion != NULL)
	{
		_expressionManager->StartMotionPriority(motion, false, PriorityForce);
	}
	else
	{
		SCE_DBG_LOG_DEBUG("[APP]expression[%s] is null ", expressionID);
	}
}

void LRModel::SetRandomExpression()
{
	if (_expressions.GetSize() == 0)
	{
		return;
	}

	csmInt32 no = rand() % _expressions.GetSize();
	csmMap<csmString, ACubismMotion*>::const_iterator map_ite;
	csmInt32 i = 0;
	for (map_ite = _expressions.Begin(); map_ite != _expressions.End(); map_ite++)
	{
		if (i == no)
		{
			csmString name = (*map_ite).First;
			SetExpression(name.GetRawString());
			return;
		}
		i++;
	}
}

SceFloat LRModel::GetProjectionCorrectionFactor()
{
	return _vitaProjectionFactor;
}

SceVoid LRModel::SetProjectionCorrectionFactor(SceFloat factor)
{
	_vitaProjectionFactor = factor;
}

void LRModel::ReloadRenderer()
{
	DeleteRenderer();

	LRGXM* gxmC = LRGXM::GetInstance();
	Rendering::CubismRenderer_GXM::GXMContext context;

	context.immContext = &gxmC->immContext;
	context.msaa = &gxmC->msaa;
	context.renderTarget = &gxmC->renderTarget;
	context.shaderPatcher = &gxmC->shaderPatcher;
	context.displayColorSurface[0] = &gxmC->displayColorSurface[0];
	context.displayColorSurface[1] = &gxmC->displayColorSurface[1];
	context.displayBufferSync[0] = &gxmC->displayBufferSync[0];
	context.displayBufferSync[1] = &gxmC->displayBufferSync[1];
	context.displayBufferData[0] = &gxmC->displayBufferData[0];
	context.displayBufferData[1] = &gxmC->displayBufferData[1];
	context.depthStencilSurface = &gxmC->depthStencilSurface;

	CreateRenderer((void *)&context);

	SetupTextures();
}

void LRModel::SetupTextures()
{
	for (csmInt32 modelTextureNumber = 0; modelTextureNumber < _modelSetting->GetTextureCount(); modelTextureNumber++)
	{
		if (strcmp(_modelSetting->GetTextureFileName(modelTextureNumber), "") == 0)
		{
			continue;
		}

		csmString texturePath = _modelSetting->GetTextureFileName(modelTextureNumber);
		texturePath = _modelHomeDir + texturePath;

		csmInt32 len = texturePath.GetLength();
		std::string str = texturePath.GetRawString();
		str.replace(len - 3, 3, "gxt");

		_textures.PushBack(vita2d_load_GXT_file((char *)str.c_str(), 0, VITA2D_IO_TYPE_NORMAL));

		//GXM
		GetRenderer<Rendering::CubismRenderer_GXM>()->BindTexture(modelTextureNumber, &_textures[modelTextureNumber]->gxm_tex);
	}

	GetRenderer<Rendering::CubismRenderer_GXM>()->IsPremultipliedAlpha(false);
}

void LRModel::MotionEventFired(const csmString& eventValue)
{
	SCE_DBG_LOG_DEBUG("%s is fired on LAppModel!!", eventValue.GetRawString());
}

Csm::Rendering::CubismOffscreenFrame_GXM& LRModel::GetRenderBuffer()
{
	return _renderBuffer;
}