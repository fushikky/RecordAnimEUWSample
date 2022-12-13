// Fill out your copyright notice in the Description page of Project Settings.


#include "RuntimeAnimRecorder.h"
#include "UObject/SavePackage.h"

// Sets default values
ARuntimeAnimRecorder::ARuntimeAnimRecorder()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ARuntimeAnimRecorder::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ARuntimeAnimRecorder::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ARuntimeAnimRecorder::PrintStr(FString InStr)
{
	GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, InStr);
}

bool ARuntimeAnimRecorder::StartRecord(USkeletalMeshComponent* Component, const FString& InAssetPath, const FString& InAssetName)
{
	FString ValidatedAssetPath = InAssetPath;
	FString ValidatedAssetName = InAssetName;

	FText InvalidPathReason;
	bool const bValidPackageName = FPackageName::IsValidLongPackageName(InAssetPath, false, &InvalidPathReason);
	UObject* Parent = bValidPackageName ? CreatePackage( *ValidatedAssetPath) : nullptr;
	if (Parent == nullptr)
	{
		Parent = CreatePackage( *ValidatedAssetPath);
	}



	UAnimSequence* const NewSeq = NewObject<UAnimSequence>(Parent, *ValidatedAssetName, RF_Public | RF_Standalone);
	if (NewSeq)
	{
		// set skeleton
		NewSeq->SetSkeleton(Component->GetSkeletalMeshAsset()->GetSkeleton());
		// Notify the asset registry
		// FAssetRegistryModule::AssetCreated(NewSeq);
		// StartRecord(Component, NewSeq);
		AnimationObject = NewSeq;
		return true;
	}
	return false;
}

// bool ARuntimeAnimRecorder::Record(USkeletalMeshComponent* Component, FTransform const& ComponentToWorld, const TArray<FTransform>& SpacesBases, const FBlendedHeapCurve& AnimationCurves, int32 FrameToAdd)
// {
// 	SkeletonRootIndex = INDEX_NONE;
// 	USkeleton* AnimSkeleton = AnimationObject->GetSkeleton();
// 
// 	
// 	FSerializedAnimation  SerializedAnimation;
// 	USkeleton* AnimSkeleton = AnimationObject->GetSkeleton();
// 
// 
// 	FRawAnimSequenceTrack& RawTrack = RawTracks[TrackIndex];
// 
// 
// 	FTransform LocalTransform = SpacesBases[BoneIndex];
// 
// 	RawTrack.PosKeys.Add(FVector3f(LocalTransform.GetTranslation()));
// 	RawTrack.RotKeys.Add(FQuat4f(LocalTransform.GetRotation()));
// 	RawTrack.ScaleKeys.Add(FVector3f(LocalTransform.GetScale3D()));  
// 	if (AnimationSerializer)
// 	{
// 		AnimationSerializer->WriteFrameData(AnimationSerializer->FramesWritten, SerializedAnimation);
// 	}
// 
// }

bool ARuntimeAnimRecorder::Stop()
{

	double StartTime, ElapsedTime = 0;
	UPackage* const Package = AnimationObject->GetOutermost();
	FString const PackageName = Package->GetName();
	FString const PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	
	StartTime = FPlatformTime::Seconds();
	
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	UPackage::SavePackage(Package, NULL, *PackageFileName, SaveArgs);
	
	ElapsedTime = FPlatformTime::Seconds() - StartTime;
	UE_LOG(LogAnimation, Log, TEXT("Animation Recorder saved %s in %0.2f seconds"), *PackageName, ElapsedTime);
	return true;
}

