#include "CapsuleComponent.h"

#include "UObject/Casts.h"

UCapsuleComponent::UCapsuleComponent()
{
}

UObject* UCapsuleComponent::Duplicate(UObject* InOuter)
{
    UCapsuleComponent* NewComponent = Cast<UCapsuleComponent>(Super::Duplicate(InOuter));
    if (NewComponent)
    {
        NewComponent->CapsuleHalfHeight = CapsuleHalfHeight;
        NewComponent->CapsuleRadius = CapsuleRadius;
    }
    return NewComponent;
}

void UCapsuleComponent::SetProperties(const TMap<FString, FString>& InProperties)
{
    Super::SetProperties(InProperties);
    const FString* TempStr = nullptr;
    TempStr = InProperties.Find(TEXT("CapsuleHalfHeight"));
    if (TempStr)
    {
        CapsuleHalfHeight = FCString::Atof(**TempStr);
    }
    TempStr = InProperties.Find(TEXT("CapsuleRadius"));
    if (TempStr)
    {
        CapsuleRadius = FCString::Atof(**TempStr);
    }
}

void UCapsuleComponent::GetProperties(TMap<FString, FString>& OutProperties) const
{
    Super::GetProperties(OutProperties);
    OutProperties.Add(TEXT("CapsuleHalfHeight"), FString::SanitizeFloat(CapsuleHalfHeight));
    OutProperties.Add(TEXT("CapsuleRadius"), FString::SanitizeFloat(CapsuleRadius));
}
