// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RuntimeAnimRecorder.generated.h"

UCLASS()
class RECORDANIMEUWSAMPLE_API ARuntimeAnimRecorder : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ARuntimeAnimRecorder();

private:
		UAnimSequence* AnimationObject;
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Test")
	void PrintStr(FString InStr);
	UFUNCTION(BlueprintCallable, Category = "Recorder")
	bool StartRecord(USkeletalMeshComponent* Component, const FString& InAssetPath, const FString& InAssetName);
	// UFUNCTION(BlueprintCallable, Category = "Recorder")
	// bool Record(USkeletalMeshComponent* Component, FTransform const& ComponentToWorld, const TArray<FTransform>& SpacesBases, const FBlendedHeapCurve& AnimationCurves, int32 FrameToAdd);
	UFUNCTION(BlueprintCallable, Category = "Recorder")
	bool Stop();

};
