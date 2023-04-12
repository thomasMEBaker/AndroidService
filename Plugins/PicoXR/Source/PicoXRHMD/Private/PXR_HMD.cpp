//Unreal® Engine, Copyright 1998 – 2022, Epic Games, Inc. All rights reserved.

#include "PXR_HMD.h"
#include "PXR_HMDRenderBridge.h"
#include "PXR_Settings.h"
#if ENGINE_MAJOR_VERSION<5
#include "PostProcess/RenderingCompositionGraph.h"
#endif
#include "XRThreadUtils.h"
#include "PXR_Input.h"
#include "IHeadMountedDisplayVulkanExtensions.h"
#include "PXR_Log.h"
#include "PXR_StereoLayer.h"
#include "PXR_HMDFunctionLibrary.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/EngineVersion.h"
#include "PXR_Utils.h"
#include "SceneRendering.h"
#include "Engine/RendererSettings.h"
#include "Components/StereoLayerComponent.h"
#if PLATFORM_ANDROID
#include "HardwareInfo.h"
#include "OpenGLDrvPrivate.h"
#include "OpenGLResources.h"
#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#include "PXR_Utils.h"
#include "PxrInput.h"
#include "VulkanRHIPrivate.h"
#include "VulkanResources.h"
#endif

float FPicoXRHMD::IpdValue = 0.f;
FName FPicoXRHMD::GetSystemName() const
{
    static FName DefaultName(TEXT("PicoXRHMD"));
    return DefaultName;
}

bool FPicoXRHMD::EnumerateTrackedDevices(TArray<int32>& OutDevices, EXRTrackedDeviceType Type /*= EXRTrackedDeviceType::Any*/)
{
	if (Type == EXRTrackedDeviceType::Any || Type == EXRTrackedDeviceType::HeadMountedDisplay)
	{
		OutDevices.Add(HMDDeviceId);
		return true;
	}
	return false;
}

void FPicoXRHMD::SetInterpupillaryDistance(float NewIPD)
{
	//TODO:SetIPD(NewIPD);
}

float FPicoXRHMD::GetInterpupillaryDistance() const
{
	return UPxr_GetIPD();
}
#if ENGINE_MAJOR_VERSION>4
bool FPicoXRHMD::GetRelativeEyePose(int32 InDeviceId, int32 InEye, FQuat& OutOrientation, FVector& OutPosition)
{
#else
bool FPicoXRHMD::GetRelativeEyePose(int32 InDeviceId, EStereoscopicPass InEye, FQuat& OutOrientation, FVector& OutPosition)
{
#endif
	
	OutOrientation = FQuat::Identity;
	OutPosition = FVector::ZeroVector;
	if (InDeviceId != HMDDeviceId)
	{
		return false;
	}
	FPXRGameFrame* CurrentFrame;
	if (IsInRenderingThread())
	{
		CurrentFrame = GameFrame_RenderThread.Get();
	}
	else if (IsInGameThread())
	{
		CurrentFrame = NextGameFrameToRender_GameThread.Get();
	}
	else
	{
		return false;
	}
	if (!CurrentFrame)
	{
		return false;
	}
#if ENGINE_MAJOR_VERSION>4
	if (InDeviceId == HMDDeviceId && (InEye == eSSE_LEFT_EYE || InEye == eSSE_RIGHT_EYE))
	{
		OutPosition = FVector(0, (InEye == eSSE_LEFT_EYE ? -.5 : .5) * GetInterpupillaryDistance() * CurrentFrame->WorldToMetersScale, 0); 
#if PLATFORM_ANDROID
		int SdkVersion = 0;
        Pxr_GetConfigInt(PXR_API_VERSION, &SdkVersion);
		if (SdkVersion >= 0x2000306)
		{
			PxrQuaternionf quaternion;
			quaternion.x = 0;
			quaternion.y = 0;
			quaternion.z = 0;
			quaternion.w = 1.0f;
			switch (InEye)
			{
			case eSSE_LEFT_EYE:
				{
					Pxr_GetEyeOrientation((int)0, &quaternion);
				}
				break;
			case eSSE_RIGHT_EYE:
				{
					Pxr_GetEyeOrientation((int)1, &quaternion);
				}
				break;
			default:
				break;
			}
			OutOrientation=FQuat(quaternion.x,quaternion.y,quaternion.z,quaternion.w);
		}
#endif
		return true;
	}
#else
	if (InDeviceId == HMDDeviceId && (InEye == eSSP_LEFT_EYE || InEye == eSSP_RIGHT_EYE))
	{
		OutPosition = FVector(0, (InEye == eSSP_LEFT_EYE ? -.5 : .5) * GetInterpupillaryDistance() * CurrentFrame->WorldToMetersScale, 0);
#if PLATFORM_ANDROID
		int SdkVersion = 0;
        Pxr_GetConfigInt(PXR_API_VERSION, &SdkVersion);
		if (SdkVersion >= 0x2000306)
		{
			PxrQuaternionf quaternion;
			quaternion.x = 0;
			quaternion.y = 0;
			quaternion.z = 0;
			quaternion.w = 1.0f;
			switch (InEye)
			{
			case eSSP_LEFT_EYE:
				{
					Pxr_GetEyeOrientation((int)0, &quaternion);
				}
				break;
			case eSSP_RIGHT_EYE:
				{
					Pxr_GetEyeOrientation((int)1, &quaternion);
				}
				break;
			default:
				break;
			}
			OutOrientation=FQuat(quaternion.z,quaternion.x,quaternion.y,quaternion.w);
		}
#endif
		return true;
	}
#endif
	
	return false;
}

bool FPicoXRHMD::GetTrackingSensorProperties(int32 InDeviceId, FQuat& OutOrientation, FVector& OutPosition, FXRSensorProperties& OutSensorProperties)
{
	check(IsInGameThread());

	FPicoXRFrustum FOV;
	switch (InDeviceId)
	{
	case 0:
		FOV.FovLeft = (LeftFrustum.FovLeft + RightFrustum.FovLeft) / 2;
		FOV.FovRight = (LeftFrustum.FovRight + RightFrustum.FovRight) / 2;
		FOV.FovUp = (LeftFrustum.FovUp + RightFrustum.FovUp) / 2;
		FOV.FovDown = (LeftFrustum.FovDown + RightFrustum.FovDown) / 2;
		FOV.Near = (LeftFrustum.Near + RightFrustum.Near) / 2;
		FOV.Far = (LeftFrustum.Far + RightFrustum.Far) / 2;
		break;
	case 1:
		FOV = LeftFrustum;
		break;
	case 2:
		FOV = RightFrustum;
		break;
	default:
		return false;
		break;
	}

	if (InDeviceId == 0)
	{
		if (GetCurrentPose(InDeviceId, OutOrientation, OutPosition))
		{
			OutSensorProperties.LeftFOV = FMath::RadiansToDegrees(FOV.FovLeft);
			OutSensorProperties.RightFOV = FMath::RadiansToDegrees(FOV.FovRight);
			OutSensorProperties.TopFOV = FMath::RadiansToDegrees(FOV.FovUp);
			OutSensorProperties.BottomFOV = FMath::RadiansToDegrees(FOV.FovDown);
			OutSensorProperties.NearPlane = FOV.Near * NextGameFrameToRender_GameThread->WorldToMetersScale;
			OutSensorProperties.FarPlane = FOV.Far * NextGameFrameToRender_GameThread->WorldToMetersScale;
			OutSensorProperties.CameraDistance = 1.0f * NextGameFrameToRender_GameThread->WorldToMetersScale;
			return true;
		}
	}
	return false;
}

void FPicoXRHMD::ResetOrientationAndPosition(float Yaw /*= 0.f*/)
{
	ResetOrientation(Yaw);
	ResetPosition();
}

void FPicoXRHMD::ResetOrientation(float Yaw /*= 0.f*/)
{
#if PLATFORM_ANDROID
 	Pxr_ResetSensor(PxrResetSensorOption::PXR_RESET_ORIENTATION);
#endif
}

void FPicoXRHMD::ResetPosition()
{
#if PLATFORM_ANDROID
 	Pxr_ResetSensor(PxrResetSensorOption::PXR_RESET_POSITION);
#endif
}

bool FPicoXRHMD::GetCurrentPose(int32 DeviceId, FQuat& CurrentOrientation, FVector& CurrentPosition)
{
	if (DeviceId != HMDDeviceId)
	{
		return false;
	}
	CurrentOrientation = FQuat::Identity;
	CurrentPosition = FVector::ZeroVector;
	FPXRGameFrame* CurrentFrame = NULL;
#if PLATFORM_ANDROID
	if (IsInRenderingThread())
	{
		CurrentFrame = GameFrame_RenderThread.Get();
	}
	else if (IsInGameThread())
	{
		CurrentFrame = NextGameFrameToRender_GameThread.Get();
	}
	else
	{
		return false;
	}
	if (CurrentFrame)
	{
		CurrentOrientation = CurrentFrame->Orientation;
		CurrentPosition = CurrentFrame->Position;
		PXR_LOGV(PxrUnreal, "GetCurrentPose Frame:%u Rotation:%s,Position:%s", CurrentFrame->FrameNumber, PLATFORM_CHAR(*CurrentOrientation.Rotator().ToString()), PLATFORM_CHAR(*CurrentPosition.ToString()));
		return true;
	}
#endif
	return false;
}

void FPicoXRHMD::UPxr_GetAngularAcceleration(FVector& AngularAcceleration)
{
	FPXRGameFrame* CurrentFrame = NULL;
	if (IsInGameThread())
	{
		CurrentFrame = NextGameFrameToRender_GameThread.Get();
	}
	else if (IsInRenderingThread())
	{
		CurrentFrame = GameFrame_RenderThread.Get();
	}
	if (CurrentFrame)
	{
		AngularAcceleration = CurrentFrame->AngularAcceleration;
	}
	else
	{
		AngularAcceleration = FVector::ZeroVector;
	}
}

void FPicoXRHMD::UPxr_GetVelocity(FVector& Velocity)
{
	FPXRGameFrame* CurrentFrame = NULL;
	if (IsInGameThread())
	{
		CurrentFrame = NextGameFrameToRender_GameThread.Get();
	}
	else if (IsInRenderingThread())
	{
		CurrentFrame = GameFrame_RenderThread.Get();
	}
	if (CurrentFrame)
	{
		Velocity = CurrentFrame->Velocity;
	}
	else
	{
		Velocity = FVector::ZeroVector;
	}
}

void FPicoXRHMD::UPxr_GetAcceleration(FVector& Acceleration)
{
	FPXRGameFrame* CurrentFrame = NULL;
	if (IsInGameThread())
	{
		CurrentFrame = NextGameFrameToRender_GameThread.Get();
	}
	else if (IsInRenderingThread())
	{
		CurrentFrame = GameFrame_RenderThread.Get();
	}
	if (CurrentFrame)
	{
		Acceleration = CurrentFrame->Acceleration;
	}
	else
	{
		Acceleration = FVector::ZeroVector;
	}
}

void FPicoXRHMD::UPxr_GetAngularVelocity(FVector& AngularVelocity)
{
	FPXRGameFrame* CurrentFrame = NULL;
	if (IsInGameThread())
	{
		CurrentFrame = NextGameFrameToRender_GameThread.Get();
	}
	else if (IsInRenderingThread())
	{
		CurrentFrame = GameFrame_RenderThread.Get();
	}
	if (CurrentFrame)
	{
		AngularVelocity = CurrentFrame->AngularVelocity;
	}
	else
	{
		AngularVelocity = FVector::ZeroVector;
	}
}

FString FPicoXRHMD::UPxr_GetDeviceModel()
{
	return DeviceModel;
}

void FPicoXRHMD::SetBaseRotation(const FRotator& BaseRot)
{
}

FRotator FPicoXRHMD::GetBaseRotation() const
{
	return FRotator::ZeroRotator;
}

void FPicoXRHMD::SetBaseOrientation(const FQuat& BaseOrient)
{
}

FQuat FPicoXRHMD::GetBaseOrientation() const
{
	return FQuat::Identity;
}

void FPicoXRHMD::SetTrackingOrigin(EHMDTrackingOrigin::Type NewOrigin)
{
#if PLATFORM_ANDROID
    switch (NewOrigin)
    {
	    case EHMDTrackingOrigin::Eye:
	    	{
				Pxr_SetTrackingOrigin(PxrTrackingOrigin::PXR_EYE_LEVEL);
	    		PXR_LOGD(PxrUnreal,"SetTrackingOrigin:EHMDTrackingOrigin::Eye");
	    		TrackingOrigin = NewOrigin;
	    		break;
	    	}
		case EHMDTrackingOrigin::Floor:
	    	{
				Pxr_SetTrackingOrigin(PxrTrackingOrigin::PXR_FLOOR_LEVEL);
	    		PXR_LOGD(PxrUnreal,"SetTrackingOrigin:EHMDTrackingOrigin::Floor");
	    		TrackingOrigin = NewOrigin;
	    		break;
	    	}
    	case EHMDTrackingOrigin::Stage:
	    	{
				Pxr_SetTrackingOrigin(PxrTrackingOrigin::PXR_STAGE_LEVEL);
	    		PXR_LOGD(PxrUnreal,"SetTrackingOrigin:EHMDTrackingOrigin::Stage");
	    		TrackingOrigin = NewOrigin;
	    		break;
	    	}
    	default:
    		break;
    }
#endif
}

EHMDTrackingOrigin::Type FPicoXRHMD::GetTrackingOrigin() const
{
	return TrackingOrigin;
}

class TSharedPtr< class IStereoRendering, ESPMode::ThreadSafe > FPicoXRHMD::GetStereoRenderingDevice()
{
	return SharedThis(this);
}

float FPicoXRHMD::GetWorldToMetersScale() const
{
	if (NextGameFrameToRender_GameThread.IsValid())
	{
		return NextGameFrameToRender_GameThread->WorldToMetersScale;
	}

	if (GWorld != nullptr)
	{
		return GWorld->GetWorldSettings()->WorldToMeters;
	}

	return 100.0f;
}

bool FPicoXRHMD::DoesSupportPositionalTracking() const
{
	return !PicoXRSetting->bIsHMD3Dof;
}

bool FPicoXRHMD::IsHMDEnabled() const
{
	return true;
}

void FPicoXRHMD::EnableHMD(bool Allow /*= true*/)
{
}

bool FPicoXRHMD::GetHMDMonitorInfo(MonitorInfo &MonitorDesc)
{
	check(IsInGameThread());
	MonitorDesc.MonitorName = FString("PicoXR Window");
	MonitorDesc.MonitorId = 0;
	MonitorDesc.DesktopX = MonitorDesc.DesktopY = 0;
	MonitorDesc.ResolutionX = MonitorDesc.ResolutionY = 0;
	MonitorDesc.WindowSizeX = MonitorDesc.WindowSizeY = 0;

	MonitorDesc.ResolutionX = MonitorDesc.WindowSizeX = RTSize.X;
	MonitorDesc.ResolutionY = MonitorDesc.WindowSizeY = RTSize.Y;

	return true;
}

void FPicoXRHMD::GetFieldOfView(float& OutHFOVInDegrees, float& OutVFOVInDegrees) const
{
	const float LeftHFOVInDegrees = FMath::RadiansToDegrees(LeftFrustum.FovRight - LeftFrustum.FovLeft);
	const float LeftVFOVInDegrees = FMath::RadiansToDegrees(LeftFrustum.FovUp - LeftFrustum.FovDown);

	const float RightHFOVInDegrees = FMath::RadiansToDegrees(RightFrustum.FovRight - RightFrustum.FovLeft);
	const float RightVFOVInDegrees = FMath::RadiansToDegrees(RightFrustum.FovUp - RightFrustum.FovDown);

	OutHFOVInDegrees = (LeftHFOVInDegrees+RightHFOVInDegrees)/2;
	OutVFOVInDegrees = (LeftVFOVInDegrees+RightVFOVInDegrees)/2;
}

bool FPicoXRHMD::IsChromaAbCorrectionEnabled() const
{
	return false;
}

FIntPoint FPicoXRHMD::GetIdealRenderTargetSize() const
{
	FIntPoint IdealRenderTargetSize;
 	IdealRenderTargetSize.X = FMath::CeilToInt(RTSize.X * PixelDensity / (bIsMobileMultiViewEnabled ? 2 : 1));
 	IdealRenderTargetSize.Y = FMath::CeilToInt(RTSize.Y * PixelDensity);

	FIntPoint MaxRenderTextureSize = FIntPoint(8192, 4096);
	if (DeviceModel.Contains("G2") && PicoXRSetting->bEnableFoveation)
	{
		PXR_LOGD(PxrUnreal, "The max RenderTexture size is (4160,2080) if the Device is G2/G2 4K !");
		MaxRenderTextureSize.X = 4160;
		MaxRenderTextureSize.Y = 2080;
	}

	const uint32 DividableBy = 4;
	const uint32 Mask = ~(DividableBy - 1);
	IdealRenderTargetSize.X = (IdealRenderTargetSize.X + DividableBy - 1) & Mask;
	IdealRenderTargetSize.Y = (IdealRenderTargetSize.Y + DividableBy - 1) & Mask;
	IdealRenderTargetSize.X = IdealRenderTargetSize.X > MaxRenderTextureSize.X ? MaxRenderTextureSize.X : IdealRenderTargetSize.X;
	IdealRenderTargetSize.Y = IdealRenderTargetSize.Y > MaxRenderTextureSize.Y ? MaxRenderTextureSize.Y : IdealRenderTargetSize.Y;

	PXR_LOGV(PxrUnreal,"GetIdealRenderTargetSize  RenderTextureSize:%d,%d", IdealRenderTargetSize.X , IdealRenderTargetSize.Y);
	return IdealRenderTargetSize;
}

void FPicoXRHMD::OnBeginRendering_GameThread()
{
}

void FPicoXRHMD::OnBeginRendering_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily)
{
}

bool FPicoXRHMD::OnStartGameFrame(FWorldContext& WorldContext)
{
	PXR_LOGV(PxrUnreal, "OnStartGameFrame");
	if (IsEngineExitRequested())
	{
		return false;
	}
	RefreshTrackingToWorldTransform(WorldContext);
	if (!WorldContext.World() || (!(GEnableVREditorHacks && WorldContext.WorldType == EWorldType::Editor) && !WorldContext.World()->IsGameWorld()))
	{
		return false;
	}
	CachedWorldToMetersScale = WorldContext.World()->GetWorldSettings()->WorldToMeters;
	OnGameFrameBegin_GameThread();
  	return true;
}

bool FPicoXRHMD::OnEndGameFrame(FWorldContext& WorldContext)
{
	FPXRGameFrame* const CurrentGameFrame = GameFrame_GameThread.Get();
	if (CurrentGameFrame)
	{
		CurrentGameFrame->TrackingToWorld = ComputeTrackingToWorldTransform(WorldContext);
	}
	else
	{
		return false;
	}
	OnGameFrameEnd_GameThread();
	return true;
}

bool FPicoXRHMD::IsStereoEnabled() const
{
	return true;
}

bool FPicoXRHMD::EnableStereo(bool Stereo /*= true*/)
{
	return true;
}
#if ENGINE_MAJOR_VERSION>4
void FPicoXRHMD::AdjustViewRect(int32 StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
#else
void FPicoXRHMD::AdjustViewRect(EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
#endif
{
	const FIntPoint RenderTargetSize = GetIdealRenderTargetSize();
	if(!bIsMobileMultiViewEnabled)
	{
		SizeX = RenderTargetSize.X / 2;
	}else
	{
		SizeX = RenderTargetSize.X;
	}
	SizeY = RenderTargetSize.Y;
#if ENGINE_MAJOR_VERSION>4
	if (StereoPass == eSSE_RIGHT_EYE && !bIsMobileMultiViewEnabled)
#else
	if (StereoPass == eSSP_RIGHT_EYE && !bIsMobileMultiViewEnabled)
#endif
	{
		X += SizeX;
	}
	PXR_LOGV(PxrUnreal,"AdjustViewRect StereoPass:%d ,X: %d,Y: %d ,SizeX: %d,SizeY: %d)", (int)StereoPass, X, Y, SizeX, SizeY);
}
#if ENGINE_MAJOR_VERSION>4
FMatrix FPicoXRHMD::GetStereoProjectionMatrix(int32 StereoPassType) const
{
	FPicoXRFrustum Frustum = (StereoPassType == eSSE_LEFT_EYE) ? LeftFrustum : RightFrustum;
	const float ProjectionCenterOffset = 0;// 0.151976421f;
	const float PassProjectionOffset = (StereoPassType == eSSE_LEFT_EYE) ? ProjectionCenterOffset : -ProjectionCenterOffset;
#else
FMatrix FPicoXRHMD::GetStereoProjectionMatrix(const enum EStereoscopicPass StereoPassType) const
{
	FPicoXRFrustum Frustum = (StereoPassType == eSSP_LEFT_EYE) ? LeftFrustum : RightFrustum;
	const float ProjectionCenterOffset = 0;// 0.151976421f;
	const float PassProjectionOffset = (StereoPassType == eSSP_LEFT_EYE) ? ProjectionCenterOffset : -ProjectionCenterOffset;
#endif
	// correct far and near planes for reversed-Z projection matrix
	const float WorldScale = GetWorldToMetersScale() * (1.0 / 100.0f); // physical scale is 100 UUs/meter
	float ZNear = GNearClippingPlane * WorldScale;
	Frustum.FovUp = tan(Frustum.FovUp);
	Frustum.FovDown = tan(Frustum.FovDown);
	Frustum.FovLeft = tan(Frustum.FovLeft);
	Frustum.FovRight = tan(Frustum.FovRight);
	float SumRL = (Frustum.FovRight + Frustum.FovLeft);
	float SumTB = (Frustum.FovUp + Frustum.FovDown);
	float InvRL = (1.0f / (Frustum.FovRight - Frustum.FovLeft));
	float InvTB = (1.0f / (Frustum.FovUp - Frustum.FovDown));
	FMatrix ProjectionMatrix = FMatrix(
		FPlane((2.0f * InvRL), 0.0f, 0.0f, 0.0f),
		FPlane(0.0f, (2.0f * InvTB), 0.0f, 0.0f),
		FPlane((SumRL * -InvRL), (SumTB * -InvTB), 0.0f, 1.0f),
		FPlane(0.0f, 0.0f, ZNear, 0.0f)) * FTranslationMatrix(FVector(PassProjectionOffset, 0, 0));
	return ProjectionMatrix;
}
#if ENGINE_MAJOR_VERSION>4
void FPicoXRHMD::GetEyeRenderParams_RenderThread(const struct FHeadMountedDisplayPassContext& Context, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const
{
	if (Context.View.StereoViewIndex == eSSE_LEFT_EYE)
	{
#else
void FPicoXRHMD::GetEyeRenderParams_RenderThread(const struct FRenderingCompositePassContext& Context, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const
{
	if (Context.View.StereoPass == eSSP_LEFT_EYE)
	{
#endif
		EyeToSrcUVOffsetValue.X = 0.0f;
		EyeToSrcUVOffsetValue.Y = 0.0f;

		EyeToSrcUVScaleValue.X = 0.5f;
		EyeToSrcUVScaleValue.Y = 1.0f;
	}
	else
	{
		EyeToSrcUVOffsetValue.X = 0.5f;
		EyeToSrcUVOffsetValue.Y = 0.0f;

		EyeToSrcUVScaleValue.X = 0.5f;
		EyeToSrcUVScaleValue.Y = 1.0f;
	}
}

FPicoXRHMD::FPicoXRHMD(const FAutoRegister&AutoRegister)
	: FHeadMountedDisplayBase(nullptr)
	, FSceneViewExtensionBase(AutoRegister)
	, NextLayerId(0)
	, inputFocusState(true)
	, DisplayRefreshRate(72.0f)
	, bIsMobileMultiViewEnabled(false)
	, PixelDensity(1)
	, RTSize(FIntPoint(4096, 2048))
	, MobileMSAAValue(0)
	, NeckOffset(FVector::ZeroVector)
	, RenderBridge(nullptr)
	, PicoXRSetting(nullptr)
	, bIsBindDelegate(false)
	, bIsEndGameFrame(false)
	, TrackingOrigin(EHMDTrackingOrigin::Eye)
	, PlayerController(nullptr)
	, PicoSplash(nullptr)
	, ContentResourceFinder(nullptr)
{
	EventManager = UPicoXREventManager::GetInstance();
	PicoXRSetting = GetMutableDefault<UPicoXRSettings>();
#if PLATFORM_ANDROID
	DeviceModel = FAndroidMisc::GetDeviceModel();
#endif
	NextGameFrameNumber = 1;
	WaitedFrameNumber = 0;
}

FPicoXRHMD::~FPicoXRHMD()
{
	UnInitialize();
}

void FPicoXRHMD::BeginXR()
{
#if PLATFORM_ANDROID
    //Frustum
    Pxr_GetFrustum(PXR_EYE_LEFT, &LeftFrustum.FovLeft, &LeftFrustum.FovRight, &LeftFrustum.FovUp, &LeftFrustum.FovDown,&LeftFrustum.Near,&LeftFrustum.Far);
    Pxr_GetFrustum(PXR_EYE_RIGHT, &RightFrustum.FovLeft, &RightFrustum.FovRight, &RightFrustum.FovUp, &RightFrustum.FovDown,&RightFrustum.Near,&RightFrustum.Far);

 	LeftFrustum.FovLeft = FMath::Atan(LeftFrustum.FovLeft/LeftFrustum.Near);
 	LeftFrustum.FovRight = FMath::Atan(LeftFrustum.FovRight/LeftFrustum.Near);
 	LeftFrustum.FovUp = FMath::Atan(LeftFrustum.FovUp/LeftFrustum.Near);
 	LeftFrustum.FovDown = FMath::Atan(LeftFrustum.FovDown/LeftFrustum.Near);

 	RightFrustum.FovLeft = FMath::Atan(RightFrustum.FovLeft/RightFrustum.Near);
 	RightFrustum.FovRight = FMath::Atan(RightFrustum.FovRight/RightFrustum.Near);
 	RightFrustum.FovUp = FMath::Atan(RightFrustum.FovUp/RightFrustum.Near);
 	RightFrustum.FovDown = FMath::Atan(RightFrustum.FovDown/RightFrustum.Near);

 	//NeckOffset
 	UpdateNeckOffset();
 	IpdValue = Pxr_GetIPD();
 	PXR_LOGI(PxrUnreal,"Startup Get ipd = %f",IpdValue);

 	ExecuteOnRenderThread([this]()
 	{
 		ExecuteOnRHIThread([this]()
        {
 			if (!Pxr_IsRunning())
            {
				if (true)
                {
					SetRefreshRate();
					Pxr_BeginXr();
					float RefreshRate = 72.0f;
					Pxr_GetDisplayRefreshRate(&RefreshRate);
					DisplayRefreshRate = RefreshRate != 0 ? RefreshRate : 72.0f;
					PXR_LOGI(PxrUnreal, "Pxr_GetDisplayRefreshRate:%f", DisplayRefreshRate);
                }
                else
                {
                    PXR_LOGF(PxrUnreal,"BeginVR: Pxr_CanBeginVR return false");
                }
            }
        });
    });
	Pxr_SetControllerEnableKey(PicoXRSetting->bEnableHomeKey, PxrControllerKeyMap::PXR_CONTROLLER_KEY_HOME);
    uint32_t device;
	Pxr_GetControllerMainInputHandle(&device);
#endif
}

void FPicoXRHMD::EndXR()
{
#if PLATFORM_ANDROID
	Pxr_SetControllerEnableKey(false, PxrControllerKeyMap::PXR_CONTROLLER_KEY_HOME);
	if (IsInGameThread())
	{
		ExecuteOnRenderThread([this]()
        {
			ExecuteOnRHIThread([this]()
	        {
				Pxr_EndXr();
	        });
        });
	}
#endif
}

FPicoXRInput* FPicoXRHMD::GetPicoXRInput()
{
	TArray<IMotionController*> MotionControllers = IModularFeatures::Get().GetModularFeatureImplementations<IMotionController>(IMotionController::GetModularFeatureName());
	for (auto MotionController : MotionControllers)
	{
		if (MotionController != nullptr && MotionController->GetMotionControllerDeviceTypeName() == FName(TEXT("PicoXRInput")))
		{
			return static_cast<FPicoXRInput*>(MotionController);
		}else
		{
			PXR_LOGE(PxrUnreal,"GetPicoXRInput failed");
		}
	}
	return nullptr;
}

FPXRGameFramePtr FPicoXRHMD::MakeNewGameFrame() const
{
	FPXRGameFramePtr Result(MakeShareable(new FPXRGameFrame()));
	Result->FrameNumber = NextGameFrameNumber;
	Result->predictedDisplayTimeMs = CurrentFramePredictedTime + 1000.0f / DisplayRefreshRate;
	Result->WorldToMetersScale = CachedWorldToMetersScale;
	Result->Flags.bSplashIsShown = PicoSplash->IsShown();
	Result->bHasWaited = NextGameFrameNumber == WaitedFrameNumber ? true : false;
	return Result;
}

void FPicoXRHMD::RefreshStereoRenderingState()
{
	check(IsInGameThread());
	// TODO:Update EyeLayer
	if (!bIsMobileMultiViewEnabled && PicoXRSetting->bEnableLateLatching)
	{
		PicoXRSetting->bEnableLateLatching = false;
	}

	static const auto AllowOcclusionQueriesCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowOcclusionQueries"));
	const bool bAllowOcclusionQueries = AllowOcclusionQueriesCVar && (AllowOcclusionQueriesCVar->GetValueOnAnyThread() != 0);
	if (bAllowOcclusionQueries && PicoXRSetting->bEnableLateLatching)
	{
		PicoXRSetting->bEnableLateLatching = false;
	}

	FPicoLayerPtr* EyeLayerFound = PXRLayerMap.Find(0);
	FPicoXRStereoLayer* EyeLayer = new FPicoXRStereoLayer(**EyeLayerFound);
	*EyeLayerFound = MakeShareable(EyeLayer);

	if (EyeLayer)
	{
		FIntPoint EyeLayerSize = GetIdealRenderTargetSize();
		EyeLayer->SetProjectionLayerParams(EyeLayerSize.X, EyeLayerSize.Y, bIsMobileMultiViewEnabled ? 2 : 1, 1, 1, RHIString);
		if (!EyeLayer->IfCanReuseLayers(PXREyeLayer_RenderThread.Get()))
		{
			AllocateEyeLayer();
		}
	}
}

bool FPicoXRHMD::Initialize()
{
#if PLATFORM_ANDROID
    PXR_LOGI(PxrUnreal, "Initialize");
	PxrInitParamData initParamData;
	initParamData.activity = (void*)FAndroidApplication::GetGameActivityThis();
	initParamData.vm =(void*) GJavaVM;
	if(PicoXRSetting->bIsHMD3Dof)
 	{
		initParamData.headdof = 0;
 	}
	else
	{
		initParamData.headdof = 1;
	}
 	if(PicoXRSetting->bIsController3Dof)
 	{
		initParamData.controllerdof = 0;
 	}
 	else
 	{
		initParamData.controllerdof = 1;
 	}
	Pxr_SetInitializeData(&initParamData);
	Pxr_SetPlatformOption(PxrPlatformOption::PXR_UNREAL);
	//GraphicOption
 	const FString HardwareDetails = FHardwareInfo::GetHardwareDetailsString();
 	const FString RHILookup = NAME_RHI.ToString() + TEXT("=");
 	if (!FParse::Value(*HardwareDetails, *RHILookup, RHIString))
 	{
 		return false;
 	}

    if (RHIString == TEXT("OpenGL")) {
        PXR_LOGI(PxrUnreal, "RHIString OpenGL");
        Pxr_SetGraphicOption(PxrGraphicOption::PXR_OPENGL_ES);
        RenderBridge = CreateRenderBridge_OpenGL(this);
        Pxr_Initialize();
    } else if (RHIString == TEXT("Vulkan")) {
        PXR_LOGI(PxrUnreal, "RHIString Vulkan");
        Pxr_SetGraphicOption(PxrGraphicOption::PXR_VULKAN);
        RenderBridge = CreateRenderBridge_Vulkan(this);
        Pxr_Initialize();
        RenderBridge->GetVulkanGraphics();
    } else {
        PXR_LOGF(PxrUnreal, "%s is not currently supported by the PicoXR runtime", PLATFORM_CHAR(*RHIString));
        return false;
    }

	if (!bIsBindDelegate)
	{
		FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddRaw(this, &FPicoXRHMD::ApplicationPauseDelegate);
		FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(this, &FPicoXRHMD::ApplicationResumeDelegate);
		bIsBindDelegate = true;
	}

    Pxr_SetColorSpace(IsMobileColorsRGB() ? PXR_COLOR_SPACE_SRGB : PXR_COLOR_SPACE_LINEAR);

    //Config MobileMultiView
    const auto CVarMobileMultiView = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.MobileMultiView"));
    bIsMobileMultiViewEnabled = (CVarMobileMultiView && CVarMobileMultiView->GetValueOnAnyThread() != 0);
    GSupportsMobileMultiView = Pxr_GetFeatureSupported(PXR_FEATURE_MULTIVIEW);
    bIsMobileMultiViewEnabled = GSupportsMobileMultiView && bIsMobileMultiViewEnabled;
	Pxr_EnableMultiview(bIsMobileMultiViewEnabled);
    PXR_LOGI(PxrUnreal, "bIsMobileMultiViewEnabled = %d", bIsMobileMultiViewEnabled);
    EnableContentProtect(PicoXRSetting->bUseContentProtect);
    FString UnrealSDKVersion = "UE4_2.0.6.8";
	FString UnrealVersion = FString::FromInt(ENGINE_MINOR_VERSION);
	UnrealSDKVersion = UnrealSDKVersion + UnrealVersion;
	PXR_LOGI(PxrUnreal, "%s,xrVersion:%s", PLATFORM_CHAR(*FEngineVersion::Current().ToString()), PLATFORM_CHAR(*UnrealSDKVersion));
	Pxr_SetConfigString(PXR_ENGINE_VERSION,TCHAR_TO_UTF8(*UnrealSDKVersion));
	
	//todo:for skylight capture error,Looking for better solutions in the future
	#if ENGINE_MINOR_VERSION ==25
	if (RHIString == TEXT("Vulkan")
			&&bIsMobileMultiViewEnabled)
	{
		GSupportsRenderTargetFormat_PF_FloatRGBA=false;
	}
    #endif
	
    SetTrackingOrigin(EHMDTrackingOrigin::Eye);
    if (PicoXRSetting->bEnableFoveation) 
	{
        PxrFoveationLevel FoveationLevel = static_cast<PxrFoveationLevel>((int32)PicoXRSetting->FoveationLevel);
        PXR_LOGI(PxrUnreal, "FoveationLevel=%d", FoveationLevel);
        Pxr_SetFoveationLevel(FoveationLevel);
    }

    //Config about OpenGL Context NoError
    bool bUseNoErrorContext = false;

#if PLATFORM_ANDROID && USE_ANDROID_EGL_NO_ERROR_CONTEXT
#if ENGINE_MINOR_VERSION > 23||ENGINE_MAJOR_VERSION>4
    if (AndroidEGL::GetInstance()->GetSupportsNoErrorContext())
#endif
    {
        bUseNoErrorContext = true;
    }
#endif

    PXR_LOGI(PxrUnreal, "bUseNoErrorContext = %d", bUseNoErrorContext);
    if (bUseNoErrorContext) {
        Pxr_SetConfigInt(PXR_UNREAL_OPENGL_NOERROR, 1);
    } else {
        Pxr_SetConfigInt(PXR_UNREAL_OPENGL_NOERROR, 0);
    }
	//Config RenderTexture Size
	static const auto CVarPixelDensity = IConsoleManager::Get().FindConsoleVariable(TEXT("vr.PixelDensity"));
	SetPixelDensity(FMath::Clamp(CVarPixelDensity->GetFloat(), 0.5f, 2.0f));
    int RenderTextureX = 0;
    int RenderTextureY = 0;
    Pxr_GetConfigInt(PXR_RENDER_TEXTURE_WIDTH, &RenderTextureX);
    Pxr_GetConfigInt(PXR_RENDER_TEXTURE_HEIGHT, &RenderTextureY);
 	RTSize.X = RenderTextureX * 2;
 	RTSize.Y = RenderTextureY;
 	PXR_LOGI(PxrUnreal,"GetConfig RenderTextureSize:(%d,%d)",RTSize.X,RTSize.Y);
	//Handle MSAA Setting
	ConfigMobileMSAA();
 	if (PicoXRSetting->bEnableEyeTracking)
	{
		EyeTracker = MakeShared<FPicoXREyeTracker>();
	}
 	IStereoLayers::FLayerDesc EyeLayerDesc;
 	EyeLayerDesc.Priority = INT_MIN;
 	EyeLayerDesc.Flags = LAYER_FLAG_TEX_CONTINUOUS_UPDATE;
 	const uint32 EyeLayerId = CreateLayer(EyeLayerDesc);
 	check(EyeLayerId == 0);

 	PicoSplash = MakeShareable(new FPXRSplash(this));
	if (PicoSplash)
	{
		PicoSplash->InitSplash();
	}

	if (PicoXRSetting->bUseAdvanceInterface)
	{
		UPicoXRHMDFunctionLibrary::PXR_SetLargeSpaceEnable(PicoXRSetting->bEnableLargeSpace);
	}

	int CurrentVersion = 0;
	Pxr_GetConfigInt(PxrConfigType::PXR_API_VERSION, &CurrentVersion);
	bWaitFrameVersion = CurrentVersion >= 0x2000304 ? true : false;
	if (!PreLoadLevelDelegate.IsValid())
	{
		PreLoadLevelDelegate = FCoreUObjectDelegates::PreLoadMap.AddRaw(this, &FPicoXRHMD::OnPreLoadMap);
	}

	ContentResourceFinder = NewObject<UPicoContentResourceFinder>();
	ContentResourceFinder->AddToRoot();

	RefreshStereoRenderingState();
 	return true;
#endif
 	return false;
}

void FPicoXRHMD::PollEvent()
{
#if PLATFORM_ANDROID
	int32 EventCount = 0;
	PxrEventDataBuffer* EventData[PXR_MAX_EVENT_COUNT];
	bool Ret = Pxr_PollEvent(PXR_MAX_EVENT_COUNT, &EventCount, EventData);
	if (Ret)
	{
		PXR_LOGD(PxrUnreal,"PollEvent EventCount :%d",EventCount);
		ProcessEvent(EventCount, EventData);
	}
#endif
}

void FPicoXRHMD::AllocateEyeLayer()
{
	check(IsInGameThread());

	ExecuteOnRenderThread([&]()
		{
			InitEyeLayer_RenderThread(GetImmediateCommandList_ForRenderCommand());

			const FXRSwapChainPtr& SwapChain = PXREyeLayer_RenderThread->GetSwapChain();
			if (SwapChain.IsValid())
			{
				const FRHITexture2D* const SwapChainTexture = SwapChain->GetTexture2DArray() ? SwapChain->GetTexture2DArray() : SwapChain->GetTexture2D();
				PXR_LOGI(PxrUnreal, "Allocating PicoXR %d x %d RenderTarget SwapChain!", SwapChainTexture->GetSizeX(), SwapChainTexture->GetSizeY());
			}
		});

	bNeedReAllocateViewportRenderTarget = true;
}

void FPicoXRHMD::InitEyeLayer_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	check(IsInRenderingThread());

	if (PXRLayerMap[0].IsValid())
	{
		FPicoLayerPtr EyeLayer = PXRLayerMap[0]->CloneMyself();
		EyeLayer->InitPXRLayer_RenderThread(RenderBridge, &DelayDeletion, RHICmdList, PXREyeLayer_RenderThread.Get());

		if (PXRLayers_RenderThread.Num() > 0)
		{
			PXRLayers_RenderThread[0] = EyeLayer;
		}
		else
		{
			PXRLayers_RenderThread.Add(EyeLayer);
		}

		DelayDeletion.AddLayerToDeferredDeletionQueue(PXREyeLayer_RenderThread);

		PXREyeLayer_RenderThread = EyeLayer;
	}
}

#if PLATFORM_ANDROID
void FPicoXRHMD::ProcessEvent(int32 EventCount, PxrEventDataBuffer** EventData)
{
	if (EventCount ==0 || !EventData)
	{
		return;
	}
	for (int i = 0; i < EventCount; i++)
	{
		PxrEventDataBuffer* Event = EventData[i];
		PXR_LOGD(PxrUnreal,"ProcessEvent EventCount:%d,EventType[%d]:%d",EventCount,i,Event->type);
		switch(Event->type)
		{
		case PXR_TYPE_EVENT_DATA_SESSION_STATE_READY:
		{
			BeginXR();
			break;
		}
		case PXR_TYPE_EVENT_DATA_SESSION_STATE_STOPPING:
		{
			EndXR();
			break;
		}
		case PXR_TYPE_EVENT_DATA_SEETHROUGH_STATE_CHANGED:
		{
			const PxrEventDataSeethroughStateChanged SeeThroughData = *reinterpret_cast<const PxrEventDataSeethroughStateChanged*>(Event);
			OnSeeThroughStateChange(SeeThroughData.state);
			break;
		}
		case PXR_TYPE_EVENT_FOVEATION_LEVEL_CHANGED:
		{
			const PxrEventDataFoveationLevelChanged FoveationData = *reinterpret_cast<const PxrEventDataFoveationLevelChanged*>(Event);
			OnFoveationLevelChange(FoveationData.level);
			break;
		}
		case PXR_TYPE_EVENT_FRUSTUM_STATE_CHANGED:
		{
			const PxrEventDataFrustumChanged FrustumData = *reinterpret_cast<const PxrEventDataFrustumChanged*>(Event);
			OnFrustumStateChange();
			break;
		}
		case PXR_TYPE_EVENT_RENDER_TEXTURE_CHANGED:
		{
			const PxrEventDataRenderTextureChanged RenderTextureChanged = *reinterpret_cast<const PxrEventDataRenderTextureChanged*>(Event);
			OnRenderTextureChange(RenderTextureChanged.width,RenderTextureChanged.height);
			break;
		}
		case PXR_TYPE_EVENT_TARGET_FRAME_RATE_STATE_CHANGED:
		{
			const PxrEventDataTargetFrameRateChanged FrameRateChanged = *reinterpret_cast<const PxrEventDataTargetFrameRateChanged*>(Event);
			OnTargetFrameRateChange(FrameRateChanged.frameRate);
			break;
		}

		case PXR_TYPE_EVENT_DATA_CONTROLLER:
		{
			const PxrEventDataControllerChanged Controller = *reinterpret_cast<const PxrEventDataControllerChanged*>(Event);
			ProcessControllerEvent(Controller);
			break;
		}

		case PXR_TYPE_EVENT_HARDIPD_STATE_CHANGED:
		{
			const PxrEventDataHardIPDStateChanged IPDState = *reinterpret_cast<const PxrEventDataHardIPDStateChanged*>(Event);
			IpdValue = IPDState.ipd;
			PXR_LOGD(PxrUnreal,"ProcessEvent PXR_TYPE_EVENT_HARDIPD_STATE_CHANGED IPD:%f",IPDState.ipd);
			EventManager->IpdChangedDelegate.Broadcast(IPDState.ipd);
			UPicoXRHMDFunctionLibrary::PicoXRIPDChangedCallback.ExecuteIfBound(IPDState.ipd);
			break;
		}
		case PXR_TYPE_EVENT_DATA_HMD_KEY:
		{
			const PxrEventDataHmdKey HomeKey = *reinterpret_cast<const PxrEventDataHmdKey*>(Event);
			EventManager->LongHomePressedDelegate.Broadcast();
			break;
		}
		case PXR_TYPE_EVENT_DATA_MRC_STATUS:
		{
			const PxrEventDataMrcStatusChanged MRC = *reinterpret_cast<const PxrEventDataMrcStatusChanged*>(Event);
			MRCEnabled = MRC.mrc_status == 0 ? true : false;
			break;
		}
		case PXR_TYPE_EVENT_DATA_REFRESH_RATE_CHANGED:
		{
			const PxrEventDataRefreshRateChanged RateState = *reinterpret_cast<const PxrEventDataRefreshRateChanged*>(Event);
			PXR_LOGD(PxrUnreal, "ProcessEvent PXR_TYPE_EVENT_DATA_REFRESH_RATE_CHANGED Rate:%f", RateState.refrashRate);
			DisplayRefreshRate = RateState.refrashRate;
			EventManager->RefreshRateChangedDelegate.Broadcast(RateState.refrashRate);
			break;
		}
		case PXR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
		{
			const PxrEventDataSessionStateChanged sessionStateChanged = *reinterpret_cast<const PxrEventDataSessionStateChanged*>(Event);
			inputFocusState = sessionStateChanged.state == PXR_SESSION_STATE_FOCUSED;
			break;
		}
		default:
			break;
		}
	}
}

void FPicoXRHMD::ProcessControllerEvent(const PxrEventDataControllerChanged EventData)
{
	PXR_LOGD(PxrUnreal,"ProcessControllerEvent eventtype:%d,Handness:%d,State:%d",EventData.eventtype,EventData.controller,EventData.status);
	FPicoXRInput* PicoInput = GetPicoXRInput();
	switch (EventData.eventtype)
	{
	case PXR_DEVICE_CONNECTCHANGED:
		{
			PXR_LOGD(PxrUnreal,"ProcessControllerEvent PXR_DEVICE_CONNECTCHANGED  controller:%d,State:%d",EventData.controller,EventData.status);
			PicoInput->OnControllerConnectChangedDelegate(EventData.controller, EventData.status);
			EventManager->DeviceConnectChangedDelegate.Broadcast(EventData.controller, EventData.status);
			break;
		}
	case PXR_DEVICE_MAIN_CHANGED:
		{
		    PXR_LOGD(PxrUnreal, "ProcessControllerEvent PXR_DEVICE_MAIN_CHANGED controller:%d", EventData.controller);
		    PicoInput->OnControllerMainChangedDelegate(EventData.controller);
		    EventManager->DeviceMainChangedDelegate.Broadcast(EventData.controller);
			break;
		}
	default:
		break;
	}
}
#endif
#if ENGINE_MAJOR_VERSION>4
void FPicoXRHMD::MakeAllStereolayerComponentsUpdate()
{
	TArray<UObject*> StereoLayerComponents;

	GetObjectsOfClass(UStereoLayerComponent::StaticClass(), StereoLayerComponents);

	for (int32 StereoLayerComponentIndex = 0; StereoLayerComponentIndex < StereoLayerComponents.Num(); ++StereoLayerComponentIndex)
	{
		UStereoLayerComponent* StereoLayerComponent = Cast<UStereoLayerComponent>(StereoLayerComponents[StereoLayerComponentIndex]);
		check(StereoLayerComponent);
		StereoLayerComponent->MarkTextureForUpdate();
	}
	PXR_LOGD(PxrUnreal, "Layer Create InLayerDesc.Texture Failed!!!!!");
}

void FPicoXRHMD::MakeAllStereoLayerComponentsDirty()
{
	TArray<UObject*> StereoLayerComponents;
	
	GetObjectsOfClass(UStereoLayerComponent::StaticClass(), StereoLayerComponents);
	
	for (int32 StereoLayerComponentIndex = 0; StereoLayerComponentIndex < StereoLayerComponents.Num(); ++StereoLayerComponentIndex)
	{
		UStereoLayerComponent* StereoLayerComponent = Cast<UStereoLayerComponent>(StereoLayerComponents[StereoLayerComponentIndex]);
		check(StereoLayerComponent);
		StereoLayerComponent->MarkStereoLayerDirty();
	}
}
#endif
void FPicoXRHMD::OnSeeThroughStateChange(int32 SeeThroughState)
{
	PXR_LOGD(PxrUnreal, "OnSeeThroughStateChange SeeThroughState:%d", SeeThroughState);
}

void FPicoXRHMD::OnFoveationLevelChange(int32 NewFoveationLevel)
{
#if PLATFORM_ANDROID
	if (!PicoXRSetting->bEnableFoveation)
	{
		PicoXRSetting->bEnableFoveation = true;
		PicoXRSetting->FoveationLevel = static_cast<EFoveationLevel::Type>(NewFoveationLevel);
	}
	PxrFoveationLevel FoveationLevel = static_cast<PxrFoveationLevel>(NewFoveationLevel);
	Pxr_SetFoveationLevel(FoveationLevel);
#endif
}

void FPicoXRHMD::OnFrustumStateChange()
{
#if PLATFORM_ANDROID
    Pxr_GetFrustum(PXR_EYE_LEFT, &LeftFrustum.FovLeft, &LeftFrustum.FovRight, &LeftFrustum.FovUp, &LeftFrustum.FovDown,&LeftFrustum.Near,&LeftFrustum.Far);
    Pxr_GetFrustum(PXR_EYE_RIGHT, &RightFrustum.FovLeft, &RightFrustum.FovRight, &RightFrustum.FovUp, &RightFrustum.FovDown,&RightFrustum.Near,&RightFrustum.Far);
 	LeftFrustum.FovLeft = FMath::Atan(LeftFrustum.FovLeft/LeftFrustum.Near);
 	LeftFrustum.FovRight = FMath::Atan(LeftFrustum.FovRight/LeftFrustum.Near);
 	LeftFrustum.FovUp = FMath::Atan(LeftFrustum.FovUp/LeftFrustum.Near);
 	LeftFrustum.FovDown = FMath::Atan(LeftFrustum.FovDown/LeftFrustum.Near);

 	RightFrustum.FovLeft = FMath::Atan(RightFrustum.FovLeft/RightFrustum.Near);
 	RightFrustum.FovRight = FMath::Atan(RightFrustum.FovRight/RightFrustum.Near);
 	RightFrustum.FovUp = FMath::Atan(RightFrustum.FovUp/RightFrustum.Near);
 	RightFrustum.FovDown = FMath::Atan(RightFrustum.FovDown/RightFrustum.Near);
#endif
}

void FPicoXRHMD::OnRenderTextureChange(int32 Width, int32 Height)
{
 	RTSize.X = Width * 2;
 	RTSize.Y = Height;
 	PXR_LOGI(PxrUnreal,"OnRenderTextureChange RenderTextureSize:(%d,%d)",RTSize.X,RTSize.Y);
}

void FPicoXRHMD::OnTargetFrameRateChange(int32 NewFrameRate)
{
 	GEngine->SetMaxFPS(NewFrameRate);
}

void FPicoXRHMD::UnInitialize()
{
#if PLATFORM_ANDROID
	Pxr_Shutdown();
#endif
	PXRLayerMap.Reset();
	PXRLayers_RenderThread.Reset();
	PXRLayers_RHIThread.Reset();
 	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
	if (PreLoadLevelDelegate.IsValid())
	{
		PreLoadLevelDelegate.Reset();
	}
 	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	if (PicoSplash)
	{
		PicoSplash->ShutDownSplash();
	}
	if (ContentResourceFinder)
	{
		ContentResourceFinder->ConditionalBeginDestroy();
		ContentResourceFinder = nullptr;
	}
}

void FPicoXRHMD::ApplicationPauseDelegate()
{
	PXR_LOGI(PxrUnreal,"FPicoXRHMD::ApplicationPauseDelegate");
	PollEvent();
}

void FPicoXRHMD::ApplicationResumeDelegate()
{
	PXR_LOGI(PxrUnreal,"FPicoXRHMD::ApplicationResumeDelegate");
	if (EventManager)
	{
		EventManager->ResumeDelegate.Broadcast();
	}
}

void FPicoXRHMD::UPxr_EnableFoveation(bool enable)
{
	if (PicoXRSetting)
	{
		PicoXRSetting->bEnableFoveation = enable;
	}
}

TSharedPtr<FPicoXREyeTracker> FPicoXRHMD::UPxr_GetEyeTracker()
{
	if (!EyeTracker)
	{
		EyeTracker = MakeShared<FPicoXREyeTracker>();
	}
 	return EyeTracker;
}

void FPicoXRHMD::UpdateNeckOffset()
{
	if (PicoXRSetting->bIsHMD3Dof && PicoXRSetting->bEnableNeckModel)
	{
		if (PicoXRSetting->bUseCustomNeckParameter)
		{
			NeckOffset = PicoXRSetting->NeckOffset;
		}
		else
		{
			NeckOffset = FVector(0.0805f,0, 0.075f);
#if ENGINE_MAJOR_VERSION>4
            FVector3f tempNeckOffset=FVector3f(0.0805f,0, 0.075f);
#if PLATFORM_ANDROID
			Pxr_GetConfigFloat(PXR_NECK_MODEL_X, &tempNeckOffset.Y);
			Pxr_GetConfigFloat(PXR_NECK_MODEL_Y, &tempNeckOffset.Z);
			Pxr_GetConfigFloat(PXR_NECK_MODEL_Z, &tempNeckOffset.X);
#endif
			NeckOffset = FVector(tempNeckOffset.X,tempNeckOffset.Y,tempNeckOffset.Z);
#else
#if PLATFORM_ANDROID
			Pxr_GetConfigFloat(PXR_NECK_MODEL_X, &NeckOffset.Y);
			Pxr_GetConfigFloat(PXR_NECK_MODEL_Y, &NeckOffset.Z);
			Pxr_GetConfigFloat(PXR_NECK_MODEL_Z, &NeckOffset.X);
#endif
		
#endif
			PicoXRSetting->NeckOffset = NeckOffset;
		}
		PXR_LOGD(PxrUnreal, "Enable Neck module NeckOffset.X is %f NeckOffset.Y is %f NeckOffset.Z is %f", NeckOffset.X, NeckOffset.Y, NeckOffset.Z);
	}
}

void FPicoXRHMD::UpdateSensorValue(FPXRGameFrame* InFrame)
{
#if PLATFORM_ANDROID
    //Position Orientation
    FPicoXRInput* PicoInput = GetPicoXRInput();
    FVector SourcePosition = FVector::ZeroVector;
	FVector LinearAcceleration = FVector::ZeroVector;
	FVector AngularAcceleration = FVector::ZeroVector;
	FVector AngularVelocity = FVector::ZeroVector;
	FVector LinearVelocity = FVector::ZeroVector;
    FQuat SourceOrientation = FQuat::Identity;
    int32 ViewNumber = 0;
    int eyeCount = 1;
    PxrPosef pose;
    
	if (bDeviceHasEnableLargeSpace&&bUserEnableLargeSpace)
	{
		PxrSensorState2 sensorState = {};
		float TrackingHeight;
		Pxr_GetPredictedMainSensorState2(InFrame->predictedDisplayTimeMs, &sensorState, &ViewNumber);
		Pxr_GetConfigFloat(PxrConfigType::PXR_TRACKING_ORIGIN_HEIGHT, &TrackingHeight);
		SourcePosition.X = sensorState.globalPose.position.x;
		SourcePosition.Y = sensorState.globalPose.position.y + TrackingHeight;
		SourcePosition.Z = sensorState.globalPose.position.z;

		SourceOrientation.X = sensorState.globalPose.orientation.x;
		SourceOrientation.Y = sensorState.globalPose.orientation.y;
		SourceOrientation.Z = sensorState.globalPose.orientation.z;
		SourceOrientation.W = sensorState.globalPose.orientation.w;
		LinearAcceleration.X = sensorState.linearAcceleration.x;
		LinearAcceleration.Y = sensorState.linearAcceleration.y;
		LinearAcceleration.Z = sensorState.linearAcceleration.z;

		AngularAcceleration.X = sensorState.angularAcceleration.x;
		AngularAcceleration.Y = sensorState.angularAcceleration.y;
		AngularAcceleration.Z = sensorState.angularAcceleration.z;

		AngularVelocity.X = sensorState.angularVelocity.x;
		AngularVelocity.Y = sensorState.angularVelocity.y;
		AngularVelocity.Z = sensorState.angularVelocity.z;

		LinearVelocity.X = sensorState.linearVelocity.x;
		LinearVelocity.Y = sensorState.linearVelocity.y;
		LinearVelocity.Z = sensorState.linearVelocity.z;
	}
	else
	{
		PxrSensorState sensorState = {};
		Pxr_GetPredictedMainSensorStateWithEyePose(InFrame->predictedDisplayTimeMs, &sensorState, &ViewNumber, eyeCount, &pose);
		SourcePosition.X = sensorState.pose.position.x;
		SourcePosition.Y = sensorState.pose.position.y;
		SourcePosition.Z = sensorState.pose.position.z;

		SourceOrientation.X = sensorState.pose.orientation.x;
		SourceOrientation.Y = sensorState.pose.orientation.y;
		SourceOrientation.Z = sensorState.pose.orientation.z;
		SourceOrientation.W = sensorState.pose.orientation.w;
		LinearAcceleration.X = sensorState.linearAcceleration.x;
		LinearAcceleration.Y = sensorState.linearAcceleration.y;
		LinearAcceleration.Z = sensorState.linearAcceleration.z;

		AngularAcceleration.X = sensorState.angularAcceleration.x;
		AngularAcceleration.Y = sensorState.angularAcceleration.y;
		AngularAcceleration.Z = sensorState.angularAcceleration.z;

		AngularVelocity.X = sensorState.angularVelocity.x;
		AngularVelocity.Y = sensorState.angularVelocity.y;
		AngularVelocity.Z = sensorState.angularVelocity.z;

		LinearVelocity.X = sensorState.linearVelocity.x;
		LinearVelocity.Y = sensorState.linearVelocity.y;
		LinearVelocity.Z = sensorState.linearVelocity.z;
	}

	FVector Position = FPicoXRUtils::ConvertXRVectorToUnrealVector(SourcePosition, InFrame->WorldToMetersScale);

	FQuat Orientation = FPicoXRUtils::ConvertXRQuatToUnrealQuat(SourceOrientation);
	// Position
	if (PicoXRSetting->bIsHMD3Dof)//head 3Dof
	{
		if (PicoXRSetting->bEnableNeckModel)
		{
			InFrame->Position = (Orientation * NeckOffset - NeckOffset.Z * FVector::UpVector);
		}
		else
		{
			InFrame->Position = FVector::ZeroVector;
		}
		//FloorLevel need add Position y
		if (TrackingOrigin == EHMDTrackingOrigin::Floor)
		{
			InFrame->Position = InFrame->Position + SourcePosition.Y * InFrame->WorldToMetersScale;
		}
	}
	else//head 6Dof
	{
		InFrame->Position = Position;
	}
	// Orientation
	InFrame->Orientation = Orientation;
	PXR_LOGV(PxrUnreal, "UpdateSensorValue:%u,PredtTime:%f,ViewNumber:%d,Rotation:%s,Position:%s", InFrame->FrameNumber, InFrame->predictedDisplayTimeMs, ViewNumber, PLATFORM_CHAR(*InFrame->Orientation.Rotator().ToString()), PLATFORM_CHAR(*InFrame->Position.ToString()));
	//velocity
	InFrame->Acceleration = LinearAcceleration;
	InFrame->AngularAcceleration = AngularAcceleration;
	InFrame->AngularVelocity = AngularVelocity;
	InFrame->Velocity = LinearVelocity;
	InFrame->ViewNumber = ViewNumber;
#endif
}

void FPicoXRHMD::UpdateSplashScreen()
 {
 	if (!GetSplash() || !IsInGameThread())
 	{
 		return;
 	}
 	if (bSplashIsShown)
 	{
 		PXR_LOGD(PxrUnreal, "UpdateSplashScreen Splash->Show()");
 	}
 	else
 	{
 		PXR_LOGD(PxrUnreal, "UpdateSplashScreen Splash->Hide()");
 	}

 }

void FPicoXRHMD::EnableContentProtect(bool bEnable)
{
#if PLATFORM_ANDROID
 	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
 	{
 		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "EnableContentProtect", "(Z)V", false);
 		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method, bEnable);
 	}
#endif
}

void FPicoXRHMD::ClearTexture_RHIThread(FRHITexture2D* SrcTexture)
{

	if (RHIString == TEXT("Vulkan"))
	{
		UE_LOG(LogHMD,Log,TEXT("Pico XR Vulkan, Cannot Clear Texture"));
		return;
	}

    check(IsInRenderingThread());

    FRHICommandListImmediate& CommandList = FRHICommandListExecutor::GetImmediateCommandList();
    const FRHIRenderPassInfo RenderPassInfo(SrcTexture, ERenderTargetActions::Clear_Store);
    CommandList.BeginRenderPass(RenderPassInfo, TEXT("ClearTexture"));
    CommandList.EndRenderPass();
    CommandList.SetViewport(0, 0, 0.0f, SrcTexture->GetSizeX(), SrcTexture->GetSizeY(), 1.0f);
#if ENGINE_MINOR_VERSION>25||ENGINE_MAJOR_VERSION>4
    CommandList.Transition(FRHITransitionInfo(SrcTexture, ERHIAccess::Unknown, ERHIAccess::SRVMask));
#else
    CommandList.TransitionResource(EResourceTransitionAccess::EReadable, SrcTexture);
#endif
}

float FPicoXRHMD::UPxr_GetIPD() const
{
 	PXR_LOGV(PxrUnreal,"const GetIPD %f", IpdValue);
 	return IpdValue;
}

void FPicoXRHMD::RenderTexture_RenderThread(class FRHICommandListImmediate& RHICmdList, class FRHITexture2D* BackBuffer, class FRHITexture2D* SrcTexture, FVector2D WindowSize) const
{
}

void FPicoXRHMD::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
#if WITH_EDITOR
	InViewFamily.EngineShowFlags.SetScreenPercentage(false);
#endif
}

void FPicoXRHMD::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	check(IsInGameThread());
}

void FPicoXRHMD::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	check(IsInGameThread());
	if (NextGameFrameToRender_GameThread.IsValid())
	{
		NextGameFrameToRender_GameThread->ShowFlags = InViewFamily.EngineShowFlags;
	}
	OnRenderFrameBegin_GameThread();
}

void FPicoXRHMD::PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
{
}

void FPicoXRHMD::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
	if (!GameFrame_RenderThread.IsValid())
	{
		return;
	}

	if (!InViewFamily.RenderTarget->GetRenderTargetTexture())
	{
		return;
	}

	OnRHIFrameBegin_RenderThread();
}

void FPicoXRHMD::PostRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
	OnRenderFrameEnd_RenderThread(RHICmdList);
}

void FPicoXRHMD::PostRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
{
}
#if ENGINE_MINOR_VERSION >26&&ENGINE_MAJOR_VERSION<5
bool FPicoXRHMD::LateLatchingEnabled() const
{
#if PLATFORM_ANDROID
	if (RHIString == TEXT("Vulkan") && PicoXRSetting)
	{
		return PicoXRSetting->bEnableLateLatching;
	}
#endif
	return false;
}

void FPicoXRHMD::PreLateLatchingViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
	check(IsInRenderingThread());
	FPXRGameFrame* CurrentFrame = GameFrame_RenderThread.Get();
	if (CurrentFrame)
	{
		PXR_LOGV(PxrUnreal, "PreLateLatchingViewFamily_RenderThread:%u", CurrentFrame->FrameNumber);
		UpdateSensorValue(CurrentFrame);
		ExecuteOnRHIThread_DoNotWait([&]()
			{
				FPXRGameFrame* CurrentFrame_RHIThread = GameFrame_RHIThread.Get();
				if (CurrentFrame_RHIThread)
				{
					CurrentFrame_RHIThread->Orientation = CurrentFrame->Orientation;
					CurrentFrame_RHIThread->Position = CurrentFrame->Position;
					CurrentFrame_RHIThread->Acceleration = CurrentFrame->Acceleration;
					CurrentFrame_RHIThread->AngularAcceleration = CurrentFrame->AngularAcceleration;
					CurrentFrame_RHIThread->AngularVelocity = CurrentFrame->AngularVelocity;
					CurrentFrame_RHIThread->Velocity = CurrentFrame->Velocity;
					CurrentFrame_RHIThread->ViewNumber = CurrentFrame->ViewNumber;
				}
			});
	}
}
#endif
#if ENGINE_MINOR_VERSION > 25||ENGINE_MAJOR_VERSION>4
bool FPicoXRHMD::AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples)
#else
bool FPicoXRHMD::AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, uint32 TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples /*= 1*/)
#endif
{
    PXR_LOGI(PxrUnreal, "AllocateRenderTargetTexture Index=%d, SizeX=%d, SizeY=%d, Format=%d, NumMips=%d, Flags=%d, TargetableTextureFlags=%d, NumSamples=%d",
                        Index, SizeX, SizeY, Format, NumMips, Flags, TargetableTextureFlags, NumSamples);

    check(IsInGameThread());
    check(IsInRenderingThread());

	check(Index == 0);

	if (PXRLayerMap[0].IsValid() && PXREyeLayer_RenderThread.IsValid())
	{
		const FXRSwapChainPtr& SwapChain = PXREyeLayer_RenderThread->GetSwapChain();
		if (SwapChain.IsValid())
		{
			OutTargetableTexture = OutShaderResourceTexture = SwapChain->GetTexture2DArray() ? SwapChain->GetTexture2DArray() : SwapChain->GetTexture2D();
			bNeedReAllocateViewportRenderTarget = false;
			return true;
		}
	}

	return false;
}

#if ENGINE_MINOR_VERSION <27&&ENGINE_MAJOR_VERSION<5
bool FPicoXRHMD::NeedReAllocateFoveationTexture(const TRefCountPtr<IPooledRenderTarget>& FoveationTarget)
#else
bool FPicoXRHMD::NeedReAllocateShadingRateTexture(const TRefCountPtr<IPooledRenderTarget>& FoveationTarget)
#endif
{
	check(IsInRenderingThread());
	return bNeedReAllocateFoveationTexture_RenderThread;
}

#if ENGINE_MINOR_VERSION < 27&&ENGINE_MAJOR_VERSION<5
#if ENGINE_MINOR_VERSION > 25
bool FPicoXRHMD::AllocateFoveationTexture(uint32 Index, uint32 RenderSizeX, uint32 RenderSizeY, uint8 Format, uint32 NumMips, ETextureCreateFlags InTexFlags, ETextureCreateFlags InTargetableTextureFlags, FTexture2DRHIRef& OutTexture, FIntPoint& OutTextureSize)
#else
bool FPicoXRHMD::AllocateFoveationTexture(uint32 Index, uint32 RenderSizeX, uint32 RenderSizeY, uint8 Format, uint32 NumMips, uint32 InTexFlags, uint32 InTargetableTextureFlags, FTexture2DRHIRef& OutTexture, FIntPoint& OutTextureSize)
#endif
#else
bool FPicoXRHMD::AllocateShadingRateTexture(uint32 Index, uint32 RenderSizeX, uint32 RenderSizeY, uint8 Format, uint32 NumMips, ETextureCreateFlags InTexFlags, ETextureCreateFlags InTargetableTextureFlags, FTexture2DRHIRef& OutTexture, FIntPoint& OutTextureSize)
#endif
{
	check(Index == 0 && (IsInRenderingThread() || IsInRHIThread()));

#if PLATFORM_ANDROID

    if (RHIString == TEXT("OpenGL") || !Pxr_GetFeatureSupported(PXR_FEATURE_FOVEATION)) 
	{
        PXR_LOGI(PxrUnreal, "AllocateFoveationTexture OpenGL Graphics & Feature Foveation is Not Supportted");
        return false;
    }

	if (PXREyeLayer_RenderThread && PXREyeLayer_RenderThread.IsValid())
    {
        const FXRSwapChainPtr& SwapChain = PXREyeLayer_RenderThread->GetFoveationSwapChain();

        if (SwapChain.IsValid())
        {
            PXR_LOGI(PxrUnreal, "AllocateFoveationTexture SwapChain.IsValid");

			FTexture2DRHIRef Texture=nullptr;
			Texture = SwapChain->GetTexture2DArray() ? SwapChain->GetTexture2DArray() : SwapChain->GetTexture2D();
            FIntPoint TexSize = Texture->GetSizeXY();

            if (Texture->IsValid() && TexSize.X > 0 && TexSize.Y > 0 )
            {
				if (bNeedReAllocateFoveationTexture_RenderThread)
				{
					bNeedReAllocateFoveationTexture_RenderThread = false;
				}

				if (RenderSizeX % TexSize.X != 0 || RenderSizeY % TexSize.Y != 0)
				{
					PXR_LOGI(PxrUnreal, "%d x %d variable resolution swapchain is not a divider of %d x %d color swapchain, potential edge problems", TexSize.X, TexSize.Y, RenderSizeX, RenderSizeY);
				}
				else
				{
					PXR_LOGI(PxrUnreal, "%d x %d variable resolution swapchain is a divider of %d x %d color swapchain, no edge problems", TexSize.X, TexSize.Y, RenderSizeX, RenderSizeY);
				}

                OutTexture = Texture;
                OutTextureSize = TexSize;
                return true;
            }
        }
    }
#endif
    return false;
}

void FPicoXRHMD::CalculateRenderTargetSize(const class FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY)
{
	InOutSizeX = GetIdealRenderTargetSize().X;
	InOutSizeY = GetIdealRenderTargetSize().Y;
}

bool FPicoXRHMD::NeedReAllocateViewportRenderTarget(const FViewport& Viewport)
{
	check(IsInGameThread());
	return bNeedReAllocateViewportRenderTarget;
}

FXRRenderBridge* FPicoXRHMD::GetActiveRenderBridge_GameThread(bool bUseSeparateRenderTarget)
{
	return RenderBridge;
}

uint32 FPicoXRHMD::CreateLayer(const FLayerDesc& InLayerDesc)
{
    check(IsInGameThread());
    uint32 LayerId = NextLayerId++;
    PXRLayerMap.Add(LayerId, MakeShareable(new FPicoXRStereoLayer(this, LayerId, InLayerDesc)));
	
	PXR_LOGD(PxrUnreal, "Layer Create LayerId=%d", LayerId);
#if ENGINE_MAJOR_VERSION>4
	if (LayerId!=0&&!InLayerDesc.Texture.IsValid())
	{
		MakeAllStereolayerComponentsUpdate();
	}
#endif
    return LayerId;
}

void FPicoXRHMD::DestroyLayer(uint32 LayerId)
{
	check(IsInGameThread());
    PXR_LOGD(PxrUnreal, "DestroyLayer LayerId=%d", LayerId);
	PXRLayerMap.Remove(LayerId);
}

void FPicoXRHMD::SetLayerDesc(uint32 LayerId, const FLayerDesc& InLayerDesc)
{
 	check(IsInGameThread());
 	FPicoLayerPtr* LayerFound = PXRLayerMap.Find(LayerId);
	if (LayerFound)
	{
		FPicoXRStereoLayer* Layer = new FPicoXRStereoLayer(**LayerFound);
		Layer->SetPXRLayerDesc(InLayerDesc);
		*LayerFound = MakeShareable(Layer);
	}
}

bool FPicoXRHMD::GetLayerDesc(uint32 LayerId, IStereoLayers::FLayerDesc& OutLayerDesc)
{
 	check(IsInGameThread());
 	FPicoLayerPtr* LayerFound = PXRLayerMap.Find(LayerId);
 	if (LayerFound)
 	{
 		OutLayerDesc = (*LayerFound)->GetPXRLayerDesc();
 		return true;
 	}
 	return false;
}

void FPicoXRHMD::MarkTextureForUpdate(uint32 LayerId)
{
    check(IsInGameThread());
    FPicoLayerPtr* LayerFound = PXRLayerMap.Find(LayerId);
    if (LayerFound)
    {
        (*LayerFound)->MarkTextureForUpdate();
    }
#if ENGINE_MAJOR_VERSION>4
	MakeAllStereoLayerComponentsDirty();
#endif
	
}

IStereoLayers::FLayerDesc FPicoXRHMD::GetDebugCanvasLayerDesc(FTextureRHIRef Texture)
{
 	IStereoLayers::FLayerDesc StereoLayerDesc;
 	StereoLayerDesc.Priority = INT_MAX;
 	StereoLayerDesc.Transform = FTransform(FVector(180.f, 0, 0));
	float scaleZ = -1.0f;
	if (RHIString == TEXT("Vulkan"))
		scaleZ = 1.0f;
 	StereoLayerDesc.Transform.SetScale3D(FVector(1,1, scaleZ));
 	StereoLayerDesc.QuadSize = FVector2D(180.f, 180.f);
 	StereoLayerDesc.LayerSize = Texture->GetTexture2D()->GetSizeXY();
 	StereoLayerDesc.PositionType = IStereoLayers::ELayerType::FaceLocked;
#if ENGINE_MINOR_VERSION >24||ENGINE_MAJOR_VERSION>4
 	StereoLayerDesc.SetShape<FQuadLayer>();
#else
 	StereoLayerDesc.ShapeType = IStereoLayers::ELayerShape::QuadLayer;
#endif
 	StereoLayerDesc.Texture = Texture;
 	StereoLayerDesc.Flags = IStereoLayers::ELayerFlags::LAYER_FLAG_TEX_CONTINUOUS_UPDATE;
 	StereoLayerDesc.Flags |= IStereoLayers::ELayerFlags::LAYER_FLAG_QUAD_PRESERVE_TEX_RATIO;
 	return StereoLayerDesc;
}

void FPicoXRHMD::GetAllocatedTexture(uint32 LayerId, FTextureRHIRef& Texture, FTextureRHIRef& LeftTexture)
{
 	Texture = LeftTexture = nullptr;
}

void FPicoXRHMD::OnBeginPlay(FWorldContext& InWorldContext)
{
	bIsSwitchingLevel = false;
	IConsoleVariable* CVSynsVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
	CVSynsVar->Set(0.0f);
	IConsoleVariable* CFCFVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FinishCurrentFrame"));
	CFCFVar->Set(0.0f);
 	PlayerController = InWorldContext.World()->GetFirstPlayerController();
 	if(EyeTracker && PicoXRSetting)
 	{
 		EyeTracker->OpenEyeTracking(PicoXRSetting->bEnableEyeTracking);
 		EyeTracker->SetEyeTrackedPlayer(PlayerController);
 	}
}

void FPicoXRHMD::OnEndPlay(FWorldContext& InWorldContext)
{
}
#if ENGINE_MINOR_VERSION >25||ENGINE_MAJOR_VERSION>4
void FPicoXRHMD::GetMotionControllerData(UObject* WorldContext, const EControllerHand Hand, FXRMotionControllerData& MotionControllerData)
{
	MotionControllerData.DeviceName = GetSystemName();
	MotionControllerData.ApplicationInstanceID = FApp::GetInstanceId();
	MotionControllerData.DeviceVisualType = EXRVisualType::Controller;
	MotionControllerData.TrackingStatus = ETrackingStatus::NotTracked;
	MotionControllerData.HandIndex = Hand;
	
	FName MotionControllerName("PicoXRInput");
	TArray<IMotionController*> MotionControllers = IModularFeatures::Get().GetModularFeatureImplementations<IMotionController>(IMotionController::GetModularFeatureName());
	FXRMotionControllerBase* MotionController = nullptr;
	for (auto Itr : MotionControllers)
	{
		if (Itr->GetMotionControllerDeviceTypeName() == MotionControllerName)
		{
			MotionController = static_cast<FXRMotionControllerBase*>(Itr);
			break;
		}
	}

	if (MotionController)
	{
		FRotator GripTrackingRotation;
		FVector GripTrackingPosition;
		MotionControllerData.bValid = MotionController->GetControllerOrientationAndPosition(0 , Hand, GripTrackingRotation, GripTrackingPosition, GetWorldToMetersScale());
		if (MotionControllerData.bValid)
		{
			MotionControllerData.TrackingStatus=ETrackingStatus::Tracked;
			const FTransform GripTrackingTransform(GripTrackingRotation.Quaternion(), GripTrackingPosition);

			MotionControllerData.GripRotation = GripTrackingTransform.GetRotation();
			MotionControllerData.GripPosition = GripTrackingTransform.GetLocation();
			
			MotionControllerData.AimPosition = MotionControllerData.GripPosition;
			MotionControllerData.AimRotation = MotionControllerData.GripRotation;
		}
	}
}

int32 FPicoXRHMD::GetXRSystemFlags() const
{
 	return EXRSystemFlags::IsHeadMounted;
}
#endif

EHMDWornState::Type FPicoXRHMD::GetHMDWornState()
{
	if (PicoXRSetting->bEnablePSensor)
	{
		int32 state = -1;
#if PLATFORM_ANDROID
		if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
		{
			static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "GetPsensorState", "()I", false);
			state = FJavaWrapper::CallIntMethod(Env, FJavaWrapper::GameActivityThis, Method);
		}
#endif
		if (state == 0)
		{
			return EHMDWornState::Worn;
		}
		else
		{
			return EHMDWornState::NotWorn;
		}
	}
		return EHMDWornState::Unknown;
}

float FPicoXRHMD::GetPixelDenity() const
{
	return PixelDensity;
}

void FPicoXRHMD::SetPixelDensity(const float NewPixelDensity)
{
	PixelDensity = FMath::Clamp(NewPixelDensity, 0.5f, 2.0f);
	PXR_LOGI(PxrUnreal, "SetPixelDensity = %f", PixelDensity);
	static const auto PixelDensityCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("vr.PixelDensity"));
	PixelDensityCVar->Set(PixelDensity);
}

void FPicoXRHMD::SetRefreshRate()
{
#if PLATFORM_ANDROID
	switch (PicoXRSetting->refreshRate)
	{
	case ERefreshRate::Default:
	{
		Pxr_SetDisplayRefreshRate(0.0f);
		break;
	}
	case ERefreshRate::RefreshRate72:
	{
		Pxr_SetDisplayRefreshRate(72.0f);
		break;
	}
	case ERefreshRate::RefreshRate90:
	{
		Pxr_SetDisplayRefreshRate(90.0f);
		break;
	}
	case ERefreshRate::RefreshRate120:
	{
		Pxr_SetDisplayRefreshRate(120.0f);
		break;
	}
	default:
		break;
	}
#endif
}

void FPicoXRHMD::ConfigMobileMSAA()
{
	const TCHAR* MSAAName=TEXT("r.MobileMSAA");
#if ENGINE_MAJOR_VERSION>4
	MSAAName=TEXT("r.MSAACount");
#endif
	//MSAA
	if (PicoXRSetting->bUseRecommendedMSAA)
	{
		MobileMSAAValue = RenderBridge->GetSystemRecommendedMSAA();
	}
	else
	{
		static const auto CVarMobileMSAA = IConsoleManager::Get().FindTConsoleVariableDataInt(MSAAName);
		MobileMSAAValue = (CVarMobileMSAA ? CVarMobileMSAA->GetValueOnAnyThread() : 2);
	}

	PXR_LOGI(PxrUnreal, "Set MSAA = %d", MobileMSAAValue);

#if PLATFORM_ANDROID
	if (RHIString == TEXT("OpenGL")) {
		int32 MaxMSAASamplesSupported = 0;

#if ENGINE_MINOR_VERSION > 24||ENGINE_MAJOR_VERSION>4
		MaxMSAASamplesSupported = FOpenGL::GetMaxMSAASamplesTileMem();
#else
		MaxMSAASamplesSupported = FAndroidOpenGL::MaxMSAASamplesTileMem;
#endif
		MobileMSAAValue = MobileMSAAValue > MaxMSAASamplesSupported ? MaxMSAASamplesSupported : MobileMSAAValue;
	}
#endif

	static IConsoleVariable* CVarMobileMSAA = IConsoleManager::Get().FindConsoleVariable(MSAAName);
	if (CVarMobileMSAA)
	{
		CVarMobileMSAA->Set(MobileMSAAValue);
	}
	PXR_LOGI(PxrUnreal, "Final MSAA = %d", MobileMSAAValue);
}

void FPicoXRHMD::UPxr_SetColorScaleAndOffset(FLinearColor ColorScale, FLinearColor ColorOffset, bool bApplyToAllLayers)
{
	PXR_LOGI(PxrUnreal,"PicoXRSetColorScaleAndOffset Scale(RGBA)= %f %f %f %f. Offset = %f %f %f %f,AllLayers = %d",ColorScale.R,ColorScale.G,ColorScale.B,ColorScale.A,ColorOffset.R,ColorOffset.G,ColorOffset.B,ColorOffset.A,bApplyToAllLayers);
	GbApplyToAllLayers = bApplyToAllLayers;
	GColorScale = ColorScale;
	GColorOffset = ColorOffset;
}

uint32 FPicoXRHMD::CreateMRCStereoLayer(FTextureRHIRef BackgroundRTTexture, FTextureRHIRef ForegroundRTTexture)
{
	IStereoLayers::FLayerDesc StereoLayerDesc;
	StereoLayerDesc.PositionType = IStereoLayers::ELayerType::FaceLocked;
	StereoLayerDesc.Texture = ForegroundRTTexture;
	StereoLayerDesc.LeftTexture = BackgroundRTTexture;
	StereoLayerDesc.Flags = IStereoLayers::ELayerFlags::LAYER_FLAG_TEX_CONTINUOUS_UPDATE | IStereoLayers::ELayerFlags::LAYER_FLAG_QUAD_PRESERVE_TEX_RATIO | IStereoLayers::ELayerFlags::LAYER_FLAG_TEX_NO_ALPHA_CHANNEL;

	check(IsInGameThread());
	if (CurrentMRCLayer)
	{
		return CurrentMRCLayer->GetID();
	}
	const uint32 LayerId = NextLayerId++;
	PXR_LOGD(PxrUnreal, "MRC Layer Create LayerId=%d", LayerId);
	CurrentMRCLayer= MakeShareable(new FPicoXRStereoLayer(this, LayerId, StereoLayerDesc));
	PXRLayerMap.Add(LayerId, CurrentMRCLayer);
	PXRLayerMap[LayerId]->bMRCLayer = true;
	MRCLayersID.Add(LayerId);
	return LayerId;
}

void FPicoXRHMD::DestroyAllMRCLayersOneTime()
{
	if (MRCLayersID.Num()>0)
	{
		for (int i=0;i<MRCLayersID.Num();i++)
		{
			DestroyLayer(MRCLayersID[i]);
		}
		MRCLayersID.Empty();
	}
}

void FPicoXRHMD::OnLargeSpaceStatusChanged(bool bEnable)
{
	PXR_LOGD(PxrUnreal, "OnLargeSpaceStatusChanged:%d", bEnable);
    bDeviceHasEnableLargeSpace = bEnable;
}

FString FPicoXRHMD::GetRHIString()
{
	return RHIString;
}

void FPicoXRHMD::OnPreLoadMap(const FString& MapName)
{
	PXR_LOGD(PxrUnreal, "OnPreLoadMap:%s", PLATFORM_CHAR(*MapName));
	bIsSwitchingLevel = true;
	if (PicoSplash)
	{
		PicoSplash->OnPreLoadMap(MapName);
	}
}

void FPicoXRHMD::WaitFrame()
{
	check(IsInGameThread());
	if (GameFrame_GameThread.IsValid())
	{
		PXR_LOGV(PxrUnreal, "WaitFrame %u", GameFrame_GameThread->FrameNumber);
		if (!PicoSplash->IsShown() && WaitedFrameNumber < GameFrame_GameThread->FrameNumber)
		{
			if (bWaitFrameVersion)
			{
#if PLATFORM_ANDROID
				Pxr_WaitFrame();
				Pxr_GetPredictedDisplayTime(&CurrentFramePredictedTime);
#endif
				GameFrame_GameThread->bHasWaited = true;
				GameFrame_GameThread->predictedDisplayTimeMs = CurrentFramePredictedTime;
				PXR_LOGV(PxrUnreal, "Pxr_GetPredictedDisplayTime after Pxr_WaitFrame %u,Time:%f", GameFrame_GameThread->FrameNumber, CurrentFramePredictedTime);
			}
			else
			{
				GameFrame_GameThread->bHasWaited = true;
			}
			WaitedFrameNumber = GameFrame_GameThread->FrameNumber;
			PXR_LOGV(PxrUnreal, "WaitFrame Wake Up %u", GameFrame_GameThread->FrameNumber);
		}
		else
		{
			PXR_LOGV(PxrUnreal, "WaitFrame not wait! %u,bSplashIsShowing:%d,WaitedFrameNumber:%u", GameFrame_GameThread->FrameNumber, PicoSplash->IsShown(), WaitedFrameNumber);
		}
	}
 }

 void FPicoXRHMD::OnGameFrameBegin_GameThread()
{
	 check(IsInGameThread());
#if PLATFORM_ANDROID
	 if (!GameFrame_GameThread.IsValid() && Pxr_IsRunning())
	 {
		 PicoSplash->SwitchActiveSplash_GameThread();
		 GameFrame_GameThread = MakeNewGameFrame();
		 NextGameFrameToRender_GameThread = GameFrame_GameThread;
		 RefreshStereoRenderingState();
		 if (!PicoSplash->IsShown())
		 {
			 UpdateSensorValue(NextGameFrameToRender_GameThread.Get());
		 }
	 }
#endif	
}

 void FPicoXRHMD::OnGameFrameEnd_GameThread()
 {
	 check(IsInGameThread());
	 WaitFrame();
	 if (GameFrame_GameThread.IsValid())
	 {
		 PXR_LOGV(PxrUnreal, "OnGameFrameEnd %u", GameFrame_GameThread->FrameNumber);
	 }
	 GameFrame_GameThread.Reset();
 }

 void FPicoXRHMD::OnRenderFrameBegin_GameThread()
 {
	 check(IsInGameThread());

	 if (NextGameFrameToRender_GameThread.IsValid() && NextGameFrameToRender_GameThread->bHasWaited && NextGameFrameToRender_GameThread!=LastGameFrameToRender_GameThread)
	 {
		 LastGameFrameToRender_GameThread = NextGameFrameToRender_GameThread;
		 NextGameFrameToRender_GameThread->Flags.bSplashIsShown = PicoSplash->IsShown();

		 if (NextGameFrameToRender_GameThread->ShowFlags.Rendering && !NextGameFrameToRender_GameThread->Flags.bSplashIsShown)
		 {
			 NextGameFrameNumber++;
		 }
		 FPXRGameFramePtr PXRFrame = NextGameFrameToRender_GameThread->CloneMyself();
		 PXR_LOGV(PxrUnreal, "OnRenderFrameBegin_GameThread %u has been eaten by render-thread!", NextGameFrameToRender_GameThread->FrameNumber);
		 TArray<FPicoLayerPtr> PXRLayers;

		 PXRLayers.Empty(PXRLayerMap.Num());

		 for (auto Pair : PXRLayerMap)
		 {
			 PXRLayers.Emplace(Pair.Value->CloneMyself());

			 if (Pair.Value->GetPXRLayerDesc().Flags & IStereoLayers::LAYER_FLAG_TEX_CONTINUOUS_UPDATE && Pair.Value->GetPXRLayerDesc().Texture.IsValid())
			 {
				 Pair.Value->MarkTextureForUpdate(true);
			 }
			 else
			 {
				 Pair.Value->MarkTextureForUpdate(false);
			 }
		 }
		 PXRLayers.Sort(FPicoLayerPtr_SortById());

		 ExecuteOnRenderThread_DoNotWait([this,PXRFrame, PXRLayers](FRHICommandListImmediate& RHICmdList)
			 {
				 if (PXRFrame.IsValid())
				 {
					 GameFrame_RenderThread = PXRFrame;

					 int32 PXRLayerIndex_Current = 0;
					 int32 PXRLastLayerIndex_RenderThread = 0;
					 TArray<FPicoLayerPtr> ValidXLayers;

					 while (PXRLayerIndex_Current < PXRLayers.Num() && PXRLastLayerIndex_RenderThread < PXRLayers_RenderThread.Num())
					 {
						 uint32 LayerIdX = PXRLayers[PXRLayerIndex_Current]->GetID();
						 uint32 LayerIdY = PXRLayers_RenderThread[PXRLastLayerIndex_RenderThread]->GetID();

						 if (LayerIdX < LayerIdY)
						 {
							 if (PXRLayers[PXRLayerIndex_Current]->InitPXRLayer_RenderThread(RenderBridge, &DelayDeletion, RHICmdList))
							 {
								 ValidXLayers.Add(PXRLayers[PXRLayerIndex_Current]);
							 }
							 PXRLayerIndex_Current++;
						 }
						 else if (LayerIdX > LayerIdY)
						 {
							 DelayDeletion.AddLayerToDeferredDeletionQueue(PXRLayers_RenderThread[PXRLastLayerIndex_RenderThread++]);
						 }
						 else
						 {
							 if (PXRLayers[PXRLayerIndex_Current]->InitPXRLayer_RenderThread(RenderBridge, &DelayDeletion, RHICmdList, PXRLayers_RenderThread[PXRLastLayerIndex_RenderThread].Get()))
							 {
								 PXRLastLayerIndex_RenderThread++;
								 ValidXLayers.Add(PXRLayers[PXRLayerIndex_Current]);
							 }
							 PXRLayerIndex_Current++;
						 }
					 }

					 while (PXRLayerIndex_Current < PXRLayers.Num())
					 {
						 if (PXRLayers[PXRLayerIndex_Current]->InitPXRLayer_RenderThread(RenderBridge, &DelayDeletion, RHICmdList))
						 {
							 ValidXLayers.Add(PXRLayers[PXRLayerIndex_Current]);
						 }
						 PXRLayerIndex_Current++;
					 }

					 while (PXRLastLayerIndex_RenderThread < PXRLayers_RenderThread.Num())
					 {
						 DelayDeletion.AddLayerToDeferredDeletionQueue(PXRLayers_RenderThread[PXRLastLayerIndex_RenderThread++]);
					 }

					 PXRLayers_RenderThread = ValidXLayers;

					 DelayDeletion.HandleLayerDeferredDeletionQueue_RenderThread();
					 if (!PicoSplash->IsShown())
					 {
						 UpdateSensorValue(GameFrame_RenderThread.Get());
					 }
				 }
			 });
	 }
 }

 void FPicoXRHMD::OnRenderFrameEnd_RenderThread(FRHICommandListImmediate& RHICmdList)
 {
	 check(IsInRenderingThread());
	 if (GameFrame_RenderThread.IsValid() && !PicoSplash->IsShown())
	 {
		 if (GameFrame_RenderThread->ShowFlags.Rendering)
		 {
			 for (int32 i = 0; i < PXRLayers_RenderThread.Num(); i++)
			 {
				 PXRLayers_RenderThread[i]->PXRLayersCopy_RenderThread(RenderBridge, RHICmdList);
			 }
		 }
	 }
	 GameFrame_RenderThread.Reset();
 }

 void FPicoXRHMD::OnRHIFrameBegin_RenderThread()
 {
	 check(IsInRenderingThread());
	 if (GameFrame_RenderThread.IsValid())
	 {
		 FPXRGameFramePtr PXRFrame = GameFrame_RenderThread->CloneMyself();
		 TArray<FPicoLayerPtr> PXRLayers = PXRLayers_RenderThread;

		 for (int32 XLayerIndex = 0; XLayerIndex < PXRLayers.Num(); XLayerIndex++)
		 {
			 PXRLayers[XLayerIndex] = PXRLayers[XLayerIndex]->CloneMyself();
		 }
		 ExecuteOnRHIThread_DoNotWait([this, PXRFrame, PXRLayers]()
			 {
				 if (PXRFrame.IsValid())
				 {
					 GameFrame_RHIThread = PXRFrame;
					 PXRLayers_RHIThread = PXRLayers;
					 PXR_LOGV(PxrUnreal, "BeginFrame %u", GameFrame_RHIThread->FrameNumber);
					 if (GameFrame_RHIThread->ShowFlags.Rendering && !GameFrame_RHIThread->Flags.bSplashIsShown) 
					 {
#if PLATFORM_ANDROID
						 if (Pxr_IsRunning())
						 {
							 Pxr_BeginFrame();
							 if (!bWaitFrameVersion)
							 {
								 Pxr_GetPredictedDisplayTime(&CurrentFramePredictedTime);
								 PXR_LOGV(PxrUnreal, "Pxr_GetPredictedDisplayTime after Pxr_BeginFrame:%f", CurrentFramePredictedTime);
							 }
							 for (int32 LayerIndex = 0; LayerIndex < PXRLayers_RHIThread.Num(); LayerIndex++)
							 {
								 PXRLayers_RHIThread[LayerIndex]->IncrementSwapChainIndex_RHIThread(RenderBridge);
							 }
						 }
						 else
						 {
							 PXR_LOGE(PxrUnreal, "Pxr Is Not Running!!!");
						 }
#endif
					 }
				 }
			 });
	 }
 }

 void FPicoXRHMD::OnRHIFrameEnd_RHIThread()
 {
	 check(IsInRHIThread() || IsInRenderingThread());
	 if (GameFrame_RHIThread.IsValid())
	 {
		 PXR_LOGV(PxrUnreal, "EndFrame %u,SubmitViewNum:%d,Rotation:%s,Position:%s", GameFrame_RHIThread->FrameNumber, GameFrame_RHIThread->ViewNumber,
			 PLATFORM_CHAR(*(GameFrame_RHIThread->Orientation.Rotator().ToString())), PLATFORM_CHAR(*(GameFrame_RHIThread->Position.ToString())));
		 if (GameFrame_RHIThread->ShowFlags.Rendering && !GameFrame_RHIThread->Flags.bSplashIsShown)
		 {
			 TArray<FPicoLayerPtr> Layers = PXRLayers_RHIThread;
			 Layers.Sort(FLayerPtr_CompareByAll());
#if PLATFORM_ANDROID
			 if (Pxr_IsRunning())
			 {
				 for (int32 LayerIndex = 0; LayerIndex < Layers.Num(); LayerIndex++)
				 {
					 if (Layers[LayerIndex]->IsVisible())
					 {
						 Layers[LayerIndex]->SubmitLayer_RHIThread(GameFrame_RHIThread.Get());
					 }
				 }
				 Pxr_EndFrame();
			 }
			 else
			 {
				 PXR_LOGE(PxrUnreal, "Pxr Is Not Running!!!");
			 }
#endif
		 }
	 }
	 GameFrame_RHIThread.Reset();
 }
