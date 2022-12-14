// Fill out your copyright notice in the Description page of Project Settings.


#include "RuntimeAnimRecorder.h"
#include "UObject/SavePackage.h"


#define LOCTEXT_NAMESPACE "FRuntimeAnimationRecoder"

static TAutoConsoleVariable<int32> CVarKeepNotifyAndCurvesOnAnimationRecord(
    TEXT("a.KeepNotifyAndCurvesOnAnimationRecord"),
    1,
    TEXT("If nonzero we keep anim notifies, curves and sync markers when animation recording, if 0 we discard them before recording."),
    ECVF_Default);


// Sets default values
ARuntimeAnimRecorder::ARuntimeAnimRecorder()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

void ARuntimeAnimRecorder::GetBoneTransforms(USkeletalMeshComponent* Component, TArray<FTransform>& BoneTransforms)
{
	const USkinnedMeshComponent* const LeaderPoseComponentInst = Component->LeaderPoseComponent.Get();
	if (LeaderPoseComponentInst)
	{
		const TArray<FTransform>& SpaceBases = LeaderPoseComponentInst->GetComponentSpaceTransforms();
		BoneTransforms.Reset(BoneTransforms.Num());
		BoneTransforms.AddUninitialized(SpaceBases.Num());
		for (int32 BoneIndex = 0; BoneIndex < SpaceBases.Num(); BoneIndex++)
		{
			if (BoneIndex < Component->GetLeaderBoneMap().Num())
			{
				int32 LeaderBoneIndex = Component->GetLeaderBoneMap()[BoneIndex];

				// If ParentBoneIndex is valid, grab matrix from LeaderPoseComponent.
				if (LeaderBoneIndex != INDEX_NONE && LeaderBoneIndex < SpaceBases.Num())
				{
					BoneTransforms[BoneIndex] = SpaceBases[LeaderBoneIndex];
				}
				else
				{
					BoneTransforms[BoneIndex] = FTransform::Identity;
				}
			}
			else
			{
				BoneTransforms[BoneIndex] = FTransform::Identity;
			}
		}
	}
	else
	{
		BoneTransforms = Component->GetComponentSpaceTransforms();
	}
}

// Called when the game starts or when spawned
void ARuntimeAnimRecorder::BeginPlay()
{
	Super::BeginPlay();

}

void ARuntimeAnimRecorder::ProcessNotifies()
{
	if (AnimationObject)
	{
		// Copy recorded notify events, animation its notify array should be empty at this point
		AnimationObject->Notifies.Append(RecordedNotifyEvents);

		// build notify tracks - first find how many tracks we want
		for (FAnimNotifyEvent& Event : AnimationObject->Notifies)
		{
			if (Event.TrackIndex >= AnimationObject->AnimNotifyTracks.Num())
			{
				AnimationObject->AnimNotifyTracks.SetNum(Event.TrackIndex + 1);

				// remake track names to create a nice sequence
				const int32 TrackNum = AnimationObject->AnimNotifyTracks.Num();
				for (int32 TrackIndex = 0; TrackIndex < TrackNum; ++TrackIndex)
				{
					FAnimNotifyTrack& Track = AnimationObject->AnimNotifyTracks[TrackIndex];
					Track.TrackName = *FString::FromInt(TrackIndex + 1);
				}
			}
		}

		// now build tracks
		for (int32 EventIndex = 0; EventIndex < AnimationObject->Notifies.Num(); ++EventIndex)
		{
			FAnimNotifyEvent& Event = AnimationObject->Notifies[EventIndex];
			AnimationObject->AnimNotifyTracks[Event.TrackIndex].Notifies.Add(&AnimationObject->Notifies[EventIndex]);
		}
	}
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

	TimePassed = 0.0;

	FString ValidatedAssetPath = InAssetPath;
	FString ValidatedAssetName = InAssetName;

	FText InvalidPathReason;
	bool const bValidPackageName = FPackageName::IsValidLongPackageName(InAssetPath, false, &InvalidPathReason);
	UObject* Parent = bValidPackageName ? CreatePackage( *ValidatedAssetPath) : nullptr;
	if (Parent == nullptr)
	{
		Parent = CreatePackage( *ValidatedAssetPath);
	}


	ARuntimeAnimRecorder::GetBoneTransforms(Component, PreviousSpacesBases);

	// UAnimSequence* AnimationObject; ê∂ê¨
	UAnimSequence* const NewSeq = NewObject<UAnimSequence>(Parent, *ValidatedAssetName, RF_Public | RF_Standalone);
	if (NewSeq)
	{
		// set skeleton
		NewSeq->SetSkeleton(Component->GetSkeletalMeshAsset()->GetSkeleton());
		// Notify the asset registry
		// FAssetRegistryModule::AssetCreated(NewSeq);
		// StartRecord(Component, NewSeq);
		AnimationObject = NewSeq;
	}

	LastFrame = 0;
	IAnimationDataController& Controller = AnimationObject->GetController();
	Controller.SetModel(AnimationObject->GetDataModel());

	Controller.OpenBracket(LOCTEXT("StartRecord_Bracket", "Starting Animation Recording"));

	const bool bKeepNotifiesAndCurves = CVarKeepNotifyAndCurvesOnAnimationRecord->GetInt() == 0 ? false : true;
	if (bKeepNotifiesAndCurves)
	{
		Controller.RemoveAllBoneTracks();
	}
	else
	{
		AnimationObject->ResetAnimation();
	}

	RecordedCurves.Reset();
	RecordedTimes.Empty();
	UIDToArrayIndexLUT = nullptr;


	USkeleton* AnimSkeleton = AnimationObject->GetSkeleton();
	// add all frames
	for (int32 BoneIndex = 0; BoneIndex < PreviousSpacesBases.Num(); ++BoneIndex)
	{
		// verify if this bone exists in skeleton
		const int32 BoneTreeIndex = AnimSkeleton->GetSkeletonBoneIndexFromMeshBoneIndex(
		                                Component->LeaderPoseComponent != nullptr ?
		                                Component->LeaderPoseComponent->GetSkinnedAsset() :
		                                Component->GetSkinnedAsset(), BoneIndex);
		if (BoneTreeIndex != INDEX_NONE)
		{
			// add tracks for the bone existing
			const FName BoneTreeName = AnimSkeleton->GetReferenceSkeleton().GetBoneName(BoneTreeIndex);
			Controller.AddBoneTrack(BoneTreeName);
			RawTracks.AddDefaulted();
		}
	}

	AnimationObject->RetargetSource = Component->GetSkeletalMeshAsset() ? AnimSkeleton->GetRetargetSourceForMesh(Component->GetSkeletalMeshAsset()) : NAME_None;
	if (AnimationObject->RetargetSource == NAME_None)
	{
		AnimationObject->RetargetSourceAsset = Component->GetSkeletalMeshAsset();
		//UpdateRetargetSourceAssetData() is protected so need to do a posteditchagned
	}

	// record the first frame
	Record(Component, PreviousComponentToWorld, PreviousSpacesBases, PreviousAnimCurves,  0);

	return false;
}

bool ARuntimeAnimRecorder::UpdateRecord(USkeletalMeshComponent* Component, float DeltaTime)
{
	PreviousAnimCurves = Component->GetAnimationCurves();

	int32 FramesRecorded = LastFrame.Value;

	TArray<FTransform> SpaceBases;

	SpaceBases = Component->GetComponentSpaceTransforms();

	TArray<FTransform> BlendedSpaceBases;
	BlendedSpaceBases.AddZeroed(SpaceBases.Num());

	FTransform BlendedComponentToWorld;

	float const PreviousTimePassed = TimePassed;
	TimePassed += DeltaTime;

	const float CurrentTime = RecordingRate.AsSeconds(FramesRecorded + 1);
	float BlendAlpha = (CurrentTime - PreviousTimePassed) / DeltaTime;
	BlendedComponentToWorld.Blend(PreviousComponentToWorld, Component->GetComponentTransform(), BlendAlpha);

	FBlendedHeapCurve BlendedCurve;

	if (!Record(Component, BlendedComponentToWorld, BlendedSpaceBases, BlendedCurve, FramesRecorded + 1))
	{
		return false;
	}

	return true;
}

bool ARuntimeAnimRecorder::Record(USkeletalMeshComponent* Component, FTransform const& ComponentToWorld, const TArray<FTransform>& SpacesBases, const FBlendedHeapCurve& AnimationCurves, int32 FrameToAdd)
{
	// AnimationRecorder.cpp#843
	if (ensure(AnimationObject))
	{
		IAnimationDataController& Controller = AnimationObject->GetController();
		USkinnedAsset* SkinnedAsset =
		    Component->LeaderPoseComponent != nullptr ?
		    Component->LeaderPoseComponent->GetSkinnedAsset() :
		    Component->GetSkinnedAsset();

		const TArray<FBoneAnimationTrack>& BoneAnimationTracks = AnimationObject->GetDataModel()->GetBoneAnimationTracks();

		if (FrameToAdd == 0)
		{
			// Find the root bone & store its transform
			SkeletonRootIndex = INDEX_NONE;
			USkeleton* AnimSkeleton = AnimationObject->GetSkeleton();

			for (const FBoneAnimationTrack& AnimationTrack : BoneAnimationTracks)
			{
				// verify if this bone exists in skeleton
				const int32 BoneTreeIndex = AnimationTrack.BoneTreeIndex;
				if (BoneTreeIndex != INDEX_NONE)
				{
					const int32 BoneIndex = AnimSkeleton->GetMeshBoneIndexFromSkeletonBoneIndex(SkinnedAsset, BoneTreeIndex);
					const int32 ParentIndex = SkinnedAsset->GetRefSkeleton().GetParentIndex(BoneIndex);
					const FTransform LocalTransform = SpacesBases[BoneIndex];
					if (ParentIndex == INDEX_NONE)
					{
						if (bRemoveRootTransform && BoneAnimationTracks.Num() > 1)
						{
							// Store initial root transform.
							// We remove the initial transform of the root bone and transform root's children
							// to remove any offset. We need to do this for sequence recording in particular
							// as we use root motion to build transform tracks that properly sync with
							// animation keyframes. If we have a transformed root bone then the assumptions
							// we make about root motion use are incorrect.
							// NEW. But we don't do this if there is just one root bone. This has come up with recording
							// single bone props and cameras.
							InitialRootTransform = LocalTransform;
							InvInitialRootTransform = LocalTransform.Inverse();
						}
						else
						{
							InitialRootTransform = InvInitialRootTransform = FTransform::Identity;
						}
						SkeletonRootIndex = BoneIndex;
						break;
					}
				}
			}
		}

		FSerializedAnimation  SerializedAnimation;
		USkeleton* AnimSkeleton = AnimationObject->GetSkeleton();

		for (int32 TrackIndex = 0; TrackIndex < BoneAnimationTracks.Num(); ++TrackIndex)
		{
			const FBoneAnimationTrack& AnimationTrack = BoneAnimationTracks[TrackIndex];
			FRawAnimSequenceTrack& RawTrack = RawTracks[TrackIndex];

			// verify if this bone exists in skeleton
			const int32 BoneTreeIndex = AnimationTrack.BoneTreeIndex;
			if (BoneTreeIndex != INDEX_NONE)
			{
				const int32 BoneIndex = AnimSkeleton->GetMeshBoneIndexFromSkeletonBoneIndex(SkinnedAsset, BoneTreeIndex);
				const int32 ParentIndex = SkinnedAsset->GetRefSkeleton().GetParentIndex(BoneIndex);

				if (bRecordTransforms)
				{
					FTransform LocalTransform = SpacesBases[BoneIndex];
					if ( ParentIndex != INDEX_NONE )
					{
						LocalTransform.SetToRelativeTransform(SpacesBases[ParentIndex]);
					}
					// if record local to world, we'd like to consider component to world to be in root
					else
					{
						if (bRecordLocalToWorld)
						{
							LocalTransform *= ComponentToWorld;
						}
					}

					RawTrack.PosKeys.Add(FVector3f(LocalTransform.GetTranslation()));
					RawTrack.RotKeys.Add(FQuat4f(LocalTransform.GetRotation()));
					RawTrack.ScaleKeys.Add(FVector3f(LocalTransform.GetScale3D()));
					// if (AnimationSerializer)
					// {
					// 	SerializedAnimation.AddTransform(TrackIndex, LocalTransform);
					// }
					// verification
					if (FrameToAdd != RawTrack.PosKeys.Num() - 1)
					{
						UE_LOG(LogAnimation, Warning, TEXT("Mismatch in animation frames. Trying to record frame: %d, but only: %d frame(s) exist. Changing skeleton while recording is not supported."), FrameToAdd, RawTrack.PosKeys.Num());
						return false;
					}
				}
				else
				{
					if (FrameToAdd == 0)
					{
						const FTransform RefPose = Component->GetSkeletalMeshAsset()->GetRefSkeleton().GetRefBonePose()[BoneIndex];
						RawTrack.PosKeys.Add((FVector3f)RefPose.GetTranslation());
						RawTrack.RotKeys.Add(FQuat4f(RefPose.GetRotation()));
						RawTrack.ScaleKeys.Add((FVector3f)RefPose.GetScale3D());
					}
				}
			}
		}

		TOptional<FQualifiedFrameTime> CurrentTime = FApp::GetCurrentFrameTime();
		RecordedTimes.Add(CurrentTime.IsSet() ? CurrentTime.GetValue() : FQualifiedFrameTime());

		// if (AnimationSerializer)
		// {
		// 	AnimationSerializer->WriteFrameData(AnimationSerializer->FramesWritten, SerializedAnimation);
		// }
		// each RecordedCurves contains all elements
		const bool bRecordCurves = bRecordMorphTargets || bRecordAttributeCurves || bRecordMaterialCurves;
		if (bRecordCurves && AnimationCurves.CurveWeights.Num() > 0)
		{
			RecordedCurves.Emplace(AnimationCurves.CurveWeights, AnimationCurves.ValidCurveWeights);
			if (UIDToArrayIndexLUT == nullptr)
			{
				UIDToArrayIndexLUT = AnimationCurves.UIDToArrayIndexLUT;
			}
			else
			{
				ensureAlways(UIDToArrayIndexLUT->Num() == AnimationCurves.UIDToArrayIndexLUT->Num());
				if (UIDToArrayIndexLUT != AnimationCurves.UIDToArrayIndexLUT)
				{
					UIDToArrayIndexLUT = AnimationCurves.UIDToArrayIndexLUT;
				}
			}
		}

		LastFrame = FrameToAdd;
	}

	return true;
}

bool ARuntimeAnimRecorder::Stop(bool bShowMessage)
{

	// double StartTime, ElapsedTime = 0;
	// UPackage* const Package = AnimationObject->GetOutermost();
	// FString const PackageName = Package->GetName();
	// FString const PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

	// StartTime = FPlatformTime::Seconds();

	// FSavePackageArgs SaveArgs;
	// SaveArgs.TopLevelFlags = RF_Standalone;
	// SaveArgs.SaveFlags = SAVE_NoError;
	// UPackage::SavePackage(Package, NULL, *PackageFileName, SaveArgs);

	// ElapsedTime = FPlatformTime::Seconds() - StartTime;
	// UE_LOG(LogAnimation, Log, TEXT("Animation Recorder saved %s in %0.2f seconds"), *PackageName, ElapsedTime);
	double StartTime, ElapsedTime = 0;

	if (AnimationObject)
	{
		IAnimationDataController& Controller = AnimationObject->GetController();
		int32 NumKeys = LastFrame.Value + 1;

		//Set Interpolation type (Step or Linear), doesn't look like there is a controller for this.
		AnimationObject->Interpolation = Interpolation;

		// can't use TimePassed. That is just total time that has been passed, not necessarily match with frame count
		Controller.SetPlayLength( (NumKeys > 1) ? RecordingRate.AsSeconds(LastFrame) : RecordingRate.AsSeconds(1) );
		Controller.SetFrameRate(RecordingRate);

		// ProcessNotifies();

		// post-process applies compression etc.
		// @todo figure out why removing redundant keys is inconsistent

		// add to real curve data
		if (RecordedCurves.Num() == NumKeys && UIDToArrayIndexLUT)
		{
			StartTime = FPlatformTime::Seconds();
			USkeleton* SkeletonObj = AnimationObject->GetSkeleton();
			for (int32 CurveUID = 0; CurveUID < UIDToArrayIndexLUT->Num(); ++CurveUID)
			{
				const int32 CurveIndex = (*UIDToArrayIndexLUT)[CurveUID];

				if (CurveIndex != MAX_uint16)
				{
					// Skip curves which type is disabled in the recorder settings
					if (const FCurveMetaData* CurveMetaData = SkeletonObj->GetCurveMetaData(CurveUID))
					{
						const bool bMorphTarget = CurveMetaData->Type.bMorphtarget;
						const bool bMaterialCurve = CurveMetaData->Type.bMaterial;
						const bool bAttributeCurve = !bMorphTarget && !bMaterialCurve;

						const FSmartNameMapping* Mapping = SkeletonObj->GetSmartNameContainer(USkeleton::AnimCurveMappingName);
						FSmartName SmartName;

						bool bShouldSkipName = false;
						if (Mapping && Mapping->FindSmartNameByUID(CurveUID, SmartName))
						{
							bShouldSkipName = ShouldSkipName(SmartName.DisplayName);
						}

						const bool bSkipCurve = (bMorphTarget && !bRecordMorphTargets) ||
						                        (bAttributeCurve && !bRecordAttributeCurves) ||
						                        (bMaterialCurve && !bRecordMaterialCurves) ||
						                        bShouldSkipName;

						if (bSkipCurve)
						{
							UE_LOG(LogAnimation, Log, TEXT("Animation Recorder skipping curve: %s"), *SmartName.DisplayName.ToString());
							continue;
						}
					}

					const FFloatCurve* FloatCurveData = nullptr;

					TArray<float> TimesToRecord;
					TArray<float> ValuesToRecord;
					TimesToRecord.SetNum(NumKeys);
					ValuesToRecord.SetNum(NumKeys);

					bool bSeenThisCurve = false;
					int32 WriteIndex = 0;
					for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
					{
						const float TimeToRecord = RecordingRate.AsSeconds(KeyIndex);
						if (RecordedCurves[KeyIndex].ValidCurveWeights[CurveIndex])
						{
							float CurCurveValue = RecordedCurves[KeyIndex].CurveWeights[CurveIndex];
							if (!bSeenThisCurve)
							{
								bSeenThisCurve = true;

								// add one and save the cache
								FSmartName CurveName;
								if (SkeletonObj->GetSmartNameByUID(USkeleton::AnimCurveMappingName, CurveUID, CurveName))
								{
									// give default curve flag for recording
									const FAnimationCurveIdentifier CurveId(CurveName, ERawCurveTrackTypes::RCT_Float);
									Controller.AddCurve(CurveId, AACF_DefaultCurve);
									FloatCurveData = AnimationObject->GetDataModel()->FindFloatCurve(CurveId);
								}
							}

							if (FloatCurveData)
							{
								TimesToRecord[WriteIndex] = TimeToRecord;
								ValuesToRecord[WriteIndex] = CurCurveValue;

								++WriteIndex;
							}
						}
					}

					// Fill all the curve data at once
					if (FloatCurveData)
					{
						TArray<FRichCurveKey> Keys;
						for (int32 Index = 0; Index < WriteIndex; ++Index)
						{
							FRichCurveKey Key(TimesToRecord[Index], ValuesToRecord[Index]);
							Key.InterpMode = InterpMode;
							Key.TangentMode = TangentMode;
							Keys.Add(Key);
						}

						const FAnimationCurveIdentifier CurveId(FloatCurveData->Name, ERawCurveTrackTypes::RCT_Float);
						Controller.SetCurveKeys(CurveId, Keys);
					}
				}
			}

			ElapsedTime = FPlatformTime::Seconds() - StartTime;
			UE_LOG(LogAnimation, Log, TEXT("Animation Recorder set keys in %0.02f seconds"), ElapsedTime);
		}

		// Populate bone tracks
		const TArray<FBoneAnimationTrack>& BoneAnimationTracks = AnimationObject->GetDataModel()->GetBoneAnimationTracks();
		for (int32 TrackIndex = 0; TrackIndex < BoneAnimationTracks.Num(); ++TrackIndex)
		{
			const FBoneAnimationTrack& AnimationTrack = BoneAnimationTracks[TrackIndex];
			const FRawAnimSequenceTrack& RawTrack = RawTracks[TrackIndex];

			FName BoneName = AnimationTrack.Name;

			bool bShouldSkipName = ShouldSkipName(AnimationTrack.Name);

			if (!bShouldSkipName)
			{
				Controller.SetBoneTrackKeys(BoneName, RawTrack.PosKeys, RawTrack.RotKeys, RawTrack.ScaleKeys);
			}
			else
			{
				TArray<FVector3f> SinglePosKey;
				SinglePosKey.Add(RawTrack.PosKeys[0]);
				TArray<FQuat4f> SingleRotKey;
				SingleRotKey.Add(RawTrack.RotKeys[0]);
				TArray<FVector3f> SingleScaleKey;
				SingleScaleKey.Add(RawTrack.ScaleKeys[0]);

				Controller.SetBoneTrackKeys(BoneName, SinglePosKey, SingleRotKey, SingleScaleKey);

				UE_LOG(LogAnimation, Log, TEXT("Animation Recorder skipping bone: %s"), *BoneName.ToString());
			}
		}

		if (bRecordTransforms == false)
		{
			Controller.RemoveAllBoneTracks();
		}

		Controller.NotifyPopulated();
		Controller.CloseBracket();


		AnimationObject->MarkPackageDirty();

		// save the package to disk, for convenience and so we can run this in standalone mode
		if (bAutoSaveAsset)
		{
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
		}

		UAnimSequence* ReturnObject = AnimationObject;

		// notify to user
		if (bShowMessage)
		{
			const FText NotificationText = FText::Format(LOCTEXT("RecordAnimation", "'{0}' has been successfully recorded [{1} keys : {2} sec(s) @ {3} Hz]"),
			                               FText::FromString(AnimationObject->GetName()),
			                               FText::AsNumber(AnimationObject->GetDataModel()->GetNumberOfKeys()),
			                               FText::AsNumber(AnimationObject->GetPlayLength()),
			                               RecordingRate.ToPrettyText()
			                                            );

			if (GIsEditor)
			{
				FNotificationInfo Info(NotificationText);
				Info.ExpireDuration = 8.0f;
				Info.bUseLargeFont = false;
				Info.Hyperlink = FSimpleDelegate::CreateLambda([ = ]()
				{
					TArray<UObject*> Assets;
					Assets.Add(ReturnObject);
					GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAssets(Assets);
				});
				Info.HyperlinkText = FText::Format(LOCTEXT("OpenNewAnimationHyperlink", "Open {0}"), FText::FromString(AnimationObject->GetName()));
				TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
				if ( Notification.IsValid() )
				{
					Notification->SetCompletionState( SNotificationItem::CS_Success );
				}
			}

			FAssetRegistryModule::AssetCreated(AnimationObject);
		}

		AnimationObject = NULL;
		PreviousSpacesBases.Empty();
		PreviousAnimCurves.Empty();

		return ReturnObject;
	}

	UniqueNotifies.Empty();
	UniqueNotifyStates.Empty();

	return nullptr;
}
}

