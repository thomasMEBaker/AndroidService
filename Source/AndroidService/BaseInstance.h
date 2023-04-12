// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "BaseInstance.generated.h"

/**
 * 
 */
UCLASS()
class ANDROIDSERVICE_API UBaseInstance : public UGameInstance
{
	GENERATED_BODY()

public:
UFUNCTION(BlueprintCallable)
	void ShowToast(const FString& Content);
	
};
