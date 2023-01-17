#include <math.h>
#include <string>
#include <Math/CubismMatrix44.hpp>
#include <Math/CubismViewMatrix.hpp>
#include <CubismFramework.hpp>

#include "LRAppLevel.hpp"
#include "LRGXM.hpp"

using namespace Csm;

namespace {
	LRAppLevel *s_instance = SCE_NULL;
}

LRAppLevel *LRAppLevel::GetInstance()
{
	if (s_instance == SCE_NULL)
	{
		s_instance = new LRAppLevel();
	}

	return s_instance;
}

SceVoid LRAppLevel::ReleaseInstance()
{
	if (s_instance != SCE_NULL)
	{
		delete s_instance;
	}

	s_instance = SCE_NULL;
}

LRAppLevel::LRAppLevel() :
	_model(NULL)
{
	_deviceToScreen = new Live2D::Cubism::Framework::CubismMatrix44();
	_viewMatrix = new Live2D::Cubism::Framework::CubismViewMatrix();
}

LRAppLevel::~LRAppLevel()
{
	delete _viewMatrix;
	delete _deviceToScreen;
}

SceVoid LRAppLevel::Initialize()
{
	SceFloat ratio = static_cast<SceFloat>(displayHeight) / static_cast<SceFloat>(displayWidth);
	SceFloat left = -1.0f;
	SceFloat right = 1.0f;
	SceFloat bottom = -ratio;
	SceFloat top = ratio;

	_viewMatrix->SetScreenRect(left, right, bottom, top);

	SceFloat screenW = fabsf(left - right);
	_deviceToScreen->LoadIdentity();
	_deviceToScreen->ScaleRelative(screenW / displayWidth, -screenW / displayWidth);
	_deviceToScreen->TranslateRelative(-displayWidth * 0.5f, -displayHeight * 0.5f);

	_viewMatrix->SetMaxScale(2.0f);
	_viewMatrix->SetMinScale(0.8f);

	_viewMatrix->SetMaxScreenRect(-2.0f, 2.0f, -2.0f, 2.0f);
}

SceVoid LRAppLevel::RenderModel(SceFloat xAngle, SceFloat yAngle, SceFloat mouth, SceFloat browLY, SceFloat browRY)
{
	CubismMatrix44 projection;
	SceFloat scaleY = static_cast<float>(displayWidth) / static_cast<float>(displayHeight);
	projection.Scale(2.0f, scaleY * 2.0f);

	if (_viewMatrix != NULL)
	{
		projection.MultiplyByMatrix(_viewMatrix);
	}

	float *proj = projection.GetArray();
	proj[13] = -2.0f;//_model->GetProjectionCorrectionFactor();

	_model->Update(xAngle, yAngle, mouth, browLY, browRY);
	_model->Draw(projection);
}

SceVoid LRAppLevel::LoadModel(std::string modelPath, std::string modelName)
{
	std::string modelJsonName = modelName;
	modelJsonName += ".model3.json";

	_model = new LRModel();
	_model->LoadAssets(modelPath.c_str(), modelJsonName.c_str());
}

SceFloat  LRAppLevel::GetDeltaTime()
{
	return s_deltaTime;
}

SceVoid LRAppLevel::UpdateTime()
{
	s_currentFrame = sceKernelGetProcessTimeWide() / (1000.0f * 1000.0f);
	s_deltaTime = s_currentFrame - s_lastFrame;
	s_lastFrame = s_currentFrame;
}