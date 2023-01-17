#pragma once

#include <vita2d_sys.h>

#include <CubismFramework.hpp>
#include <Model/CubismUserModel.hpp>
#include <ICubismModelSetting.hpp>
#include <Type/csmRectF.hpp>
#include <Rendering/GXM/CubismOffscreenSurface_GXM.hpp>

class LRModel : public Csm::CubismUserModel
{
public:

	const Csm::csmChar* MotionGroupIdle = "Idle";
	const Csm::csmChar* MotionGroupTapBody = "TapBody";

	const Csm::csmChar* HitAreaNameHead = "Head";
	const Csm::csmChar* HitAreaNameBody = "Body";

	const Csm::csmInt32 PriorityNone = 0;
	const Csm::csmInt32 PriorityIdle = 1;
	const Csm::csmInt32 PriorityNormal = 2;
	const Csm::csmInt32 PriorityForce = 3;

	LRModel();

	virtual ~LRModel();

	void LoadAssets(const Csm::csmChar* dir, const  Csm::csmChar* fileName);

	void ReloadRenderer();

	void Update(Csm::csmFloat32 xAngle, Csm::csmFloat32 yAngle, Csm::csmFloat32 mouth, Csm::csmFloat32 browLY, Csm::csmFloat32 browRY);

	void Draw(Csm::CubismMatrix44& matrix);

	Csm::CubismMotionQueueEntryHandle StartMotion(const Csm::csmChar* group, Csm::csmInt32 no, Csm::csmInt32 priority, Csm::ACubismMotion::FinishedMotionCallback onFinishedMotionHandler = NULL);

	Csm::CubismMotionQueueEntryHandle StartRandomMotion(const Csm::csmChar* group, Csm::csmInt32 priority, Csm::ACubismMotion::FinishedMotionCallback onFinishedMotionHandler = NULL);

	void SetExpression(const Csm::csmChar* expressionID);

	void SetRandomExpression();

	SceFloat GetProjectionCorrectionFactor();

	SceVoid SetProjectionCorrectionFactor(SceFloat factor);

	virtual void MotionEventFired(const Live2D::Cubism::Framework::csmString& eventValue);

	virtual Csm::csmBool HitTest(const Csm::csmChar* hitAreaName, Csm::csmFloat32 x, Csm::csmFloat32 y);

	Csm::Rendering::CubismOffscreenFrame_GXM& GetRenderBuffer();

protected:

	void DoDraw();

private:

	void SetupModel(Csm::ICubismModelSetting* setting);

	void SetupTextures();

	void PreloadMotionGroup(const Csm::csmChar* group);

	void ReleaseMotionGroup(const Csm::csmChar* group) const;

	void ReleaseMotions();

	void ReleaseExpressions();

	Csm::ICubismModelSetting* _modelSetting;
	Csm::csmString _modelHomeDir;
	Csm::csmFloat32 _userTimeSeconds;
	Csm::csmVector<Csm::CubismIdHandle> _eyeBlinkIds;
	Csm::csmVector<Csm::CubismIdHandle> _lipSyncIds;
	Csm::csmMap<Csm::csmString, Csm::ACubismMotion*>   _motions;
	Csm::csmMap<Csm::csmString, Csm::ACubismMotion*>   _expressions;
	Csm::csmVector<Csm::csmRectF> _hitArea;
	Csm::csmVector<Csm::csmRectF> _userArea;
	const Csm::CubismId* _idParamAngleX;
	const Csm::CubismId* _idParamAngleY;
	const Csm::CubismId* _idParamAngleZ;
	const Csm::CubismId* _idParamBodyAngleX;
	const Csm::CubismId* _idParamEyeBallX;
	const Csm::CubismId* _idParamEyeBallY;
	const Csm::CubismId* _idParamBrowLY;
	const Csm::CubismId* _idParamBrowRY;
	const Csm::CubismId* _idParamMouthOpenY;

	Csm::csmFloat32 _vitaProjectionFactor;
	Csm::csmVector<vita2d_texture *> _textures;

	Csm::Rendering::CubismOffscreenFrame_GXM _renderBuffer;
};

