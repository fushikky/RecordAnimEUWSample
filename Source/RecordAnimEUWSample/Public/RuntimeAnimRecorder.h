// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RuntimeAnimRecorder.generated.h"



struct FSerializedAnimationPerFrame
{
	FSerializedAnimationPerFrame() = default;

	friend FArchive& operator<<(FArchive& Ar, FSerializedAnimationPerFrame& Animation)
	{
		Ar << Animation.BoneIndex;
		Ar << Animation.PosKey;
		Ar << Animation.RotKey;
		Ar << Animation.ScaleKey;
		return Ar;
	}

	int32 BoneIndex;
	FVector PosKey;
	FQuat RotKey;
	FVector ScaleKey;

};

struct FSerializedAnimation
{
	FSerializedAnimation() = default;

	void AddTransform(int32 BoneIndex, const FTransform& InTransform)
	{
		FSerializedAnimationPerFrame Frame;
		Frame.BoneIndex = BoneIndex;
		Frame.PosKey = InTransform.GetTranslation();
		Frame.RotKey = InTransform.GetRotation();
		Frame.ScaleKey = InTransform.GetScale3D();
		AnimationData.Emplace(Frame);
	}

	friend FArchive& operator<<(FArchive& Ar, FSerializedAnimation& Animation)
	{
		Ar << Animation.AnimationData;
		return Ar;
	}
	TArray<FSerializedAnimationPerFrame> AnimationData;

};
UCLASS()
class RECORDANIMEUWSAMPLE_API ARuntimeAnimRecorder : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ARuntimeAnimRecorder();

public:
	/** Helper function to get space bases depending on master pose component */
	static void GetBoneTransforms(USkeletalMeshComponent* Component, TArray<FTransform>& BoneTransforms);

private:
	UAnimSequence* AnimationObject;
	FBlendedHeapCurve PreviousAnimCurves;
	FFrameNumber LastFrame;
	double TimePassed;

	FFrameRate RecordingRate;
	FTransform PreviousComponentToWorld;
	int32 SkeletonRootIndex;
	FTransform InvInitialRootTransform;
	FTransform InitialRootTransform;
	TArray<FTransform> PreviousSpacesBases;

	/** If true, the root bone transform will be removed from all bone transforms */
	uint8 bRemoveRootTransform;

	/** Whether or not to record transforms*/
	uint8 bRecordTransforms : 1;
	/** If true, it will record root to include LocalToWorld */
	uint8 bRecordLocalToWorld : 1;
	/** Whether or not to record morph targets*/
	uint8 bRecordMorphTargets : 1;
	/** Serializer, if set we also store data out incrementally while running*/
	// FAnimationSerializer* AnimationSerializer;
	/** Whether or not to record attribute curves*/
	uint8 bRecordAttributeCurves : 1;
	/** Whether or not to record material curves*/
	uint8 bRecordMaterialCurves : 1;

	/** Interpolation type for the recorded sequence */
	EAnimInterpolationType Interpolation;

	/** Notify events recorded at any point, processed and inserted into animation when recording has finished */
	TArray<FAnimNotifyEvent> RecordedNotifyEvents;

	/** Array of times recorded */
	TArray<FQualifiedFrameTime> RecordedTimes;
	TArray<uint16> const* UIDToArrayIndexLUT;
	TArray<FRawAnimSequenceTrack> RawTracks;

	// recording curve data
	struct FBlendedCurve
	{
		template<typename Allocator>
		FBlendedCurve(TArray<float, Allocator> CW, TBitArray<Allocator> VCW)
		{
			CurveWeights = CW;
			ValidCurveWeights = VCW;
		}

		TArray<float> CurveWeights;
		TBitArray<> ValidCurveWeights;
	};
	TArray<FBlendedCurve> RecordedCurves;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

private:
	void ProcessNotifies();

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Test")
	void PrintStr(FString InStr);
	UFUNCTION(BlueprintCallable, Category = "Recorder")
	bool StartRecord(USkeletalMeshComponent* Component, const FString& InAssetPath, const FString& InAssetName);
	UFUNCTION(BlueprintCallable, Category = "Recorder")
	bool UpdateRecord(USkeletalMeshComponent* Component, float DeltaTime);
	UFUNCTION(BlueprintCallable, Category = "Recorder")
	bool Stop(bool bShowMessage);


private:
	bool Record(USkeletalMeshComponent* Component, FTransform const& ComponentToWorld, const TArray<FTransform>& SpacesBases, const FBlendedHeapCurve& AnimationCurves, int32 FrameToAdd);

};
