//Unreal® Engine, Copyright 1998 – 2022, Epic Games, Inc. All rights reserved.

#pragma once
#include "CoreMinimal.h"
#include "PXR_Splash.h"
#include "IStereoLayers.h"
#include "XRRenderBridge.h"
#include "PXR_EyeTracker.h"
#include "PXR_StereoLayer.h"
#include "SceneViewExtension.h"
#include "PXR_EventManager.h"
#include "XRTrackingSystemBase.h"
#include "XRRenderTargetManager.h"
#include "HeadMountedDisplayBase.h"
#include "Engine/Public/SceneUtils.h"
#include "PXR_GameFrame.h"
#include "StereoLayerManager.h"
#include "PXR_DelayDeleteLayer.h"
#if PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#include "Android/AndroidPlatformMisc.h"
#include "PxrApi.h"
#endif

class FPicoXRRenderBridge;
class UPicoContentResourceFinder;

struct FPicoXRFrustum
{
	float FovLeft;
	float FovRight;
	float FovUp;
	float FovDown;
	float Near;
	float Far;

	FPicoXRFrustum()
	{
		FovUp    = 0.88119f;
		FovDown  = -0.88119f;
		FovLeft  = -0.88119f;
		FovRight = 0.88119f;
		Near     = 0.0508f;
		Far      = 100;
	}
	FString ToString() const
	{
		return
			TEXT(" FPicoXRFrustum Left : ") + FString::SanitizeFloat(FovLeft) +
			TEXT(" FPicoXRFrustum Right : ") + FString::SanitizeFloat(FovRight) +
			TEXT(" FPicoXRFrustum Up : ") + FString::SanitizeFloat(FovUp) +
			TEXT(" FPicoXRFrustum Down : ") + FString::SanitizeFloat(FovDown) +
			TEXT(" FPicoXRFrustum H : ") + FString::SanitizeFloat(FovRight - FovLeft) +
			TEXT(" FPicoXRFrustum V : ") + FString::SanitizeFloat(FovUp - FovDown )+
			TEXT(" FPicoXRFrustum Near : ") + FString::SanitizeFloat(Near) +
            TEXT(" FPicoXRFrustum Far : ") + FString::SanitizeFloat(Far) ;
	}
};

struct FCurrentFrameValue
{
	FVector CurrentPosition;
	FQuat CurrentOrientation;
	FVector AngularVelocity;
	FVector Acceleration;
	FVector AngularAcceleration;
	FVector Velocity;
	float IPD;
	int32 ViewNumber;
	float WorldToMetersScale;
	FTransform TrackingToWorld;		// for overlay
	FCurrentFrameValue()
	{
		IPD                 = 0.063f;
		ViewNumber          = 0;
		WorldToMetersScale  = 100.0f;
		CurrentPosition     = FVector::ZeroVector;
		CurrentOrientation  = FQuat::Identity;
		AngularVelocity     = FVector::ZeroVector;
		Acceleration        = FVector::ZeroVector;
		AngularAcceleration = FVector::ZeroVector;
		Velocity            = FVector::ZeroVector;
		TrackingToWorld     = FTransform::Identity;
	}
	FString ToString() const
	{
		return
			TEXT(" CurrentPosition : ") + CurrentPosition.ToString() +
            TEXT(" CurrentOrientation : ") + CurrentOrientation.ToString() +
			TEXT(" AngularVelocity : ") + AngularVelocity.ToString() +
			TEXT(" Acceleration : ") + Acceleration.ToString() +
			TEXT(" AngularAcceleration : ") + AngularAcceleration.ToString() +
			TEXT(" Velocity : ") + Velocity.ToString() +
            TEXT(" IPD: ") + FString::SanitizeFloat(IPD) +
            TEXT(" ViewNumber : ") + FString::FromInt(ViewNumber) +
			TEXT(" WorldToMetersScale : ") + FString::SanitizeFloat(WorldToMetersScale);
	}
};

/**
 * PicoXR Head Mounted Display
 */
class FPicoXRHMD : public FHeadMountedDisplayBase, public FXRRenderTargetManager, public FSceneViewExtensionBase , public IStereoLayers
{
public:
	/** IXRTrackingSystem interface */
	virtual FName GetSystemName() const override;
	virtual bool EnumerateTrackedDevices(TArray<int32>& OutDevices, EXRTrackedDeviceType Type = EXRTrackedDeviceType::Any) override;
	virtual void ResetOrientationAndPosition(float yaw = 0.f) override;
	virtual void ResetOrientation(float Yaw = 0.f) override;
	virtual void ResetPosition() override;
	virtual void SetBaseRotation(const FRotator& BaseRot) override;
	virtual FRotator GetBaseRotation() const override;
	virtual void SetBaseOrientation(const FQuat& BaseOrient) override;
	virtual FQuat GetBaseOrientation() const override;
	virtual bool GetCurrentPose(int32 DeviceId, FQuat& CurrentOrientation, FVector& CurrentPosition) override;
	virtual class IHeadMountedDisplay* GetHMDDevice() override { return this;};
	virtual class TSharedPtr< class IStereoRendering, ESPMode::ThreadSafe > GetStereoRenderingDevice() override;
	virtual float GetWorldToMetersScale() const override;
	virtual void OnBeginRendering_GameThread() override;
	virtual void OnBeginRendering_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily) override;
	virtual bool OnStartGameFrame(FWorldContext& WorldContext) override;
	virtual bool OnEndGameFrame(FWorldContext& WorldContext) override;
	virtual class IXRLoadingScreen* CreateLoadingScreen() override { return GetSplash().Get(); }
	virtual void OnBeginPlay(FWorldContext& InWorldContext) override;
	virtual void OnEndPlay(FWorldContext& InWorldContext) override;
#if ENGINE_MINOR_VERSION >25||ENGINE_MAJOR_VERSION>4
	virtual void GetMotionControllerData(UObject* WorldContext, const EControllerHand Hand, FXRMotionControllerData& MotionControllerData) override;
	virtual int32 GetXRSystemFlags() const override;
#endif

	/** IHeadMountedDisplay interface */
	virtual void SetInterpupillaryDistance(float NewInterpupillaryDistance) override;
	virtual float GetInterpupillaryDistance() const override;
	virtual bool IsHMDConnected() override { return true; }
	virtual bool IsHMDEnabled() const override;
	virtual void EnableHMD(bool allow = true) override;
	virtual bool GetHMDMonitorInfo(MonitorInfo &MonitorDesc) override;
	virtual void GetFieldOfView(float& OutHFOVInDegrees, float& OutVFOVInDegrees) const override;
	virtual bool IsChromaAbCorrectionEnabled() const override;
	virtual FIntPoint GetIdealRenderTargetSize() const override;
#if ENGINE_MAJOR_VERSION>4
	virtual void GetEyeRenderParams_RenderThread(const struct FHeadMountedDisplayPassContext& Context, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const override;
#else
	virtual void GetEyeRenderParams_RenderThread(const struct FRenderingCompositePassContext& Context, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const override;
#endif
	virtual EHMDWornState::Type GetHMDWornState() override;
	virtual float GetPixelDenity() const override;
	virtual void SetPixelDensity(const float NewPixelDensity) override;

	/** FXRTrackingSystemBase interface */
#if ENGINE_MAJOR_VERSION>4
	virtual bool GetRelativeEyePose(int32 InDeviceId, int32 ViewIndex,FQuat& OutOrientation, FVector& OutPosition) override;
#else
	virtual bool GetRelativeEyePose(int32 InDeviceId, EStereoscopicPass InEye, FQuat& OutOrientation, FVector& OutPosition) override;
#endif
	virtual	bool GetTrackingSensorProperties(int32 InDeviceId, FQuat& OutOrientation, FVector& OutPosition, FXRSensorProperties& OutSensorProperties) override;
	virtual void SetTrackingOrigin(EHMDTrackingOrigin::Type NewOrigin) override;
	virtual EHMDTrackingOrigin::Type GetTrackingOrigin() const override;
	virtual bool DoesSupportPositionalTracking() const override;

	/** FHeadMountedDisplayBase interface */
	virtual bool GetHMDDistortionEnabled(EShadingPath ShadingPath) const override { return false; }

	/** IStereoRendering interface */
	virtual bool IsStereoEnabled() const override;
	virtual bool EnableStereo(bool stereo = true) override;
#if ENGINE_MAJOR_VERSION>4
	virtual void AdjustViewRect(int32 ViewIndex, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const  override;
	virtual FMatrix GetStereoProjectionMatrix(int32 ViewIndex) const override;
#else
	virtual void AdjustViewRect(EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const override;
	virtual FMatrix GetStereoProjectionMatrix(const enum EStereoscopicPass StereoPassType) const override;
#endif
	virtual IStereoRenderTargetManager* GetRenderTargetManager() override { return this; }
	virtual void RenderTexture_RenderThread(class FRHICommandListImmediate& RHICmdList, class FRHITexture2D* BackBuffer, class FRHITexture2D* SrcTexture, FVector2D WindowSize) const override;

	/** ISceneViewExtension interface */
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override;
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override;
	virtual void PostRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override;
	virtual void PostRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override;
	virtual int32 GetPriority() const override { return -1; }
#if ENGINE_MINOR_VERSION >26&&ENGINE_MAJOR_VERSION<5
	virtual bool LateLatchingEnabled() const override;
	virtual void PreLateLatchingViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override;
#endif

	/** IStereoRenderTargetManager interface */
	virtual bool ShouldUseSeparateRenderTarget() const override { return true; }
#if ENGINE_MINOR_VERSION >25||ENGINE_MAJOR_VERSION>4
	virtual bool AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, ETextureCreateFlags InTexFlags, ETextureCreateFlags InTargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples = 1) override;
#else
	virtual bool AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, uint32 TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples = 1) override;
#endif

#if ENGINE_MINOR_VERSION <27&&ENGINE_MAJOR_VERSION<5
	virtual bool NeedReAllocateFoveationTexture(const TRefCountPtr<IPooledRenderTarget>& FoveationTarget) override;
#else
	virtual bool NeedReAllocateShadingRateTexture(const TRefCountPtr<IPooledRenderTarget>& FoveationTarget) override;
#endif

#if ENGINE_MINOR_VERSION <27&&ENGINE_MAJOR_VERSION<5
#if ENGINE_MINOR_VERSION >25
	virtual bool AllocateFoveationTexture(uint32 Index, uint32 RenderSizeX, uint32 RenderSizeY, uint8 Format, uint32 NumMips, ETextureCreateFlags InTexFlags, ETextureCreateFlags InTargetableTextureFlags, FTexture2DRHIRef& OutTexture, FIntPoint& OutTextureSize) override;
#else
	virtual bool AllocateFoveationTexture(uint32 Index, uint32 RenderSizeX, uint32 RenderSizeY, uint8 Format, uint32 NumMips, uint32 InTexFlags, uint32 InTargetableTextureFlags, FTexture2DRHIRef& OutTexture, FIntPoint& OutTextureSize) override;
#endif
#else
	virtual bool AllocateShadingRateTexture(uint32 Index, uint32 RenderSizeX, uint32 RenderSizeY, uint8 Format, uint32 NumMips, ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTexture, FIntPoint& OutTextureSize) override;
#endif

  /** FXRRenderTargetManager interface */
	virtual void CalculateRenderTargetSize(const class FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY)override;
	virtual bool NeedReAllocateViewportRenderTarget(const FViewport& Viewport) override;
	virtual FXRRenderBridge* GetActiveRenderBridge_GameThread(bool bUseSeparateRenderTarget) override;

	virtual IStereoLayers* GetStereoLayers() override { return this; }
	virtual uint32 CreateLayer(const IStereoLayers::FLayerDesc& InLayerDesc) override;
	virtual void DestroyLayer(uint32 LayerId) override;
	virtual void SetLayerDesc(uint32 LayerId, const IStereoLayers::FLayerDesc& InLayerDesc) override;
	virtual bool GetLayerDesc(uint32 LayerId, IStereoLayers::FLayerDesc& OutLayerDesc) override;
	virtual void MarkTextureForUpdate(uint32 LayerId) override;
	virtual void UpdateSplashScreen() override;
	virtual FLayerDesc GetDebugCanvasLayerDesc(FTextureRHIRef Texture) override;
	virtual void GetAllocatedTexture(uint32 LayerId, FTextureRHIRef &Texture, FTextureRHIRef &LeftTexture) override;
	virtual bool ShouldCopyDebugLayersToSpectatorScreen() const override { return true; }

	FPicoXRHMD(const FAutoRegister&);
	virtual ~FPicoXRHMD();
	void BeginXR();
	void EndXR();
	class FPicoXRInput* GetPicoXRInput();
	float UPxr_GetIPD() const;

	FPicoXRSplashPtr GetSplash() const {return  PicoSplash;};

	bool Initialize();
	void UnInitialize();
	FPicoXRRenderBridge* GetCustomRenderBridge() const { return RenderBridge; }
	void UPxr_EnableFoveation(bool enable);

	void UPxr_GetAngularAcceleration(FVector& AngularAcceleration);
	void UPxr_GetVelocity(FVector& Velocity);
	void UPxr_GetAcceleration(FVector& Acceleration);
	void UPxr_GetAngularVelocity(FVector& AngularVelocity);
	FString UPxr_GetDeviceModel();
	TSharedPtr<FPicoXREyeTracker> UPxr_GetEyeTracker();
	void ClearTexture_RHIThread(FRHITexture2D* SrcTexture);
	void UPxr_SetColorScaleAndOffset(FLinearColor ColorScale, FLinearColor ColorOffset, bool bApplyToAllLayers = false);
	uint32 CreateMRCStereoLayer(FTextureRHIRef BackgroundRTTexture, FTextureRHIRef ForegroundRTTexture);
	void DestroyAllMRCLayersOneTime();

	void OnLargeSpaceStatusChanged(bool bEnable);
	FString GetRHIString();
	int32 GetMSAAValue() { return MobileMSAAValue; }

	void OnPreLoadMap(const FString& MapName);
	FDelegateHandle PreLoadLevelDelegate;
	bool bIsSwitchingLevel;
	void WaitFrame();
	void OnGameFrameBegin_GameThread();
	void OnGameFrameEnd_GameThread();
	void OnRenderFrameBegin_GameThread();
	void OnRenderFrameEnd_RenderThread(FRHICommandListImmediate& RHICmdList);
	void OnRHIFrameBegin_RenderThread();
	void OnRHIFrameEnd_RHIThread();
	FPXRGameFramePtr MakeNewGameFrame() const;
	void RefreshStereoRenderingState();
	// Game thread
	uint32 NextGameFrameNumber;
	uint32 WaitedFrameNumber;
	FPXRGameFramePtr GameFrame_GameThread;
	FPXRGameFramePtr NextGameFrameToRender_GameThread;
	FPXRGameFramePtr LastGameFrameToRender_GameThread;
	TMap<uint32, FPicoLayerPtr> PXRLayerMap;
	FPicoLayerPtr CurrentMRCLayer;
	TArray<uint32> MRCLayersID;
	// Render thread
	FPXRGameFramePtr GameFrame_RenderThread;
	TArray<FPicoLayerPtr> PXRLayers_RenderThread;
	FPicoLayerPtr PXREyeLayer_RenderThread;
	// RHI thread
	FPXRGameFramePtr GameFrame_RHIThread;
	TArray<FPicoLayerPtr> PXRLayers_RHIThread;
	double CurrentFramePredictedTime = 0;
	bool bWaitFrameVersion = false;
	float CachedWorldToMetersScale = 100.0f;


	UPicoXREventManager* EventManager;
	uint32 NextLayerId;
	bool MRCEnabled=false;
	FLinearColor GColorScale = FLinearColor(1.0,1.0,1.0,1.0);
	FLinearColor GColorOffset = FLinearColor(0.0,0.0,0.0,0.0);
    bool GbApplyToAllLayers = false;
	bool bNeedReAllocateFoveationTexture_RenderThread = false;
	bool bNeedReAllocateViewportRenderTarget;
	bool bUserEnableLargeSpace=false;
	bool bDeviceHasEnableLargeSpace = false;
	bool inputFocusState = true;

	void PollEvent();
	UPicoContentResourceFinder* GetContentResourceFinder(){return ContentResourceFinder;}
	void AllocateEyeLayer();

	FDelayDeleteLayerManager DelayDeletion;
	void UpdateSensorValue(FPXRGameFrame* InFrame);
	double DisplayRefreshRate;
protected:
	void InitEyeLayer_RenderThread(FRHICommandListImmediate& RHICmdList);

private:
#if ENGINE_MAJOR_VERSION>4
	void MakeAllStereolayerComponentsUpdate();
	void MakeAllStereoLayerComponentsDirty();
#endif
#if PLATFORM_ANDROID
	void ProcessEvent(int EventCount, PxrEventDataBuffer** EventData);
	void ProcessControllerEvent( const PxrEventDataControllerChanged EventData);
#endif
	void OnSeeThroughStateChange(int32 SeeThroughState);
	void OnFoveationLevelChange(int32 FoveationLevel);
	void OnFrustumStateChange();
	void OnRenderTextureChange(int32 Width,int32 Height);
	void OnTargetFrameRateChange(int32 NewFrameRate);
	void ApplicationPauseDelegate();
	void ApplicationResumeDelegate();
	void UpdateNeckOffset();
	void EnableContentProtect(bool bEnable );
	void SetRefreshRate();
	void ConfigMobileMSAA();

	bool bIsMobileMultiViewEnabled;
	float PixelDensity;
	FIntPoint RTSize;
	int32 MobileMSAAValue;
	FVector NeckOffset;
	FPicoXRFrustum LeftFrustum;
	FPicoXRFrustum RightFrustum;
	TRefCountPtr<FPicoXRRenderBridge> RenderBridge;
	class UPicoXRSettings* PicoXRSetting;
	bool bIsBindDelegate;
	FString RHIString;
	bool bIsEndGameFrame;
	EHMDTrackingOrigin::Type TrackingOrigin;
	static float IpdValue;
	TSharedPtr<FPicoXREyeTracker> EyeTracker;
	APlayerController* PlayerController;
	FPicoXRSplashPtr PicoSplash;
	FString DeviceModel;
	UPicoContentResourceFinder* ContentResourceFinder;
};

