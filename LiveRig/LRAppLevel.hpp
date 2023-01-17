#pragma once

#include <string>
#include <scetypes.h>

#include <Math/CubismMatrix44.hpp>
#include <Math/CubismViewMatrix.hpp>
#include <CubismFramework.hpp>

#include "LRModel.hpp"

static SceFloat s_currentFrame;
static SceFloat s_lastFrame;
static SceFloat s_deltaTime;

class LRAppLevel
{
public:

	static LRAppLevel *GetInstance();

	static SceVoid ReleaseInstance();

	static SceFloat  GetDeltaTime();

	static SceVoid UpdateTime();

	SceVoid Initialize();

	SceVoid LoadModel(std::string modelPath, std::string modelName);

	SceVoid RenderModel(SceFloat xAngle, SceFloat yAngle, SceFloat mouth, SceFloat browLY, SceFloat browRY);

private:

	Csm::CubismMatrix44* _deviceToScreen;
	Csm::CubismViewMatrix* _viewMatrix;

	LRModel *_model;

	LRAppLevel();

	~LRAppLevel();
};

