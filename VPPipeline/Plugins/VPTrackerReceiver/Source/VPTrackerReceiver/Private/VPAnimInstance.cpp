#include "VPAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

void UVPAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (TrackingData.bIsValid)
	{
		ApplyBlendshapesToMorphTargets();
	}
}

void UVPAnimInstance::ApplyBlendshapesToMorphTargets()
{
	USkeletalMeshComponent* MeshComp = GetSkelMeshComponent();
	if (!MeshComp)
	{
		return;
	}

	for (const auto& Pair : TrackingData.FaceData.Blendshapes)
	{
		// Skip _neutral (index 0, always 0.0)
		if (Pair.Key == FName("_neutral"))
		{
			continue;
		}

		const float ClampedValue = FMath::Clamp(Pair.Value * FaceBlendWeight, 0.0f, 1.0f);
		MeshComp->SetMorphTarget(Pair.Key, ClampedValue);
	}
}
