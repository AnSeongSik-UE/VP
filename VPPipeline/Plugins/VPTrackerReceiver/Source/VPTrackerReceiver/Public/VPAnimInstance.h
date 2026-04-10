#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "VPTrackingData.h"
#include "VPAnimInstance.generated.h"

/**
 * Custom AnimInstance that applies tracking data to skeletal mesh morph targets.
 * Assign this as the Anim Class on your avatar's Skeletal Mesh Component.
 */
UCLASS()
class VPTRACKERRECEIVER_API UVPAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Current tracking data (updated every frame by the receiver) */
	UPROPERTY(BlueprintReadWrite, Category = "VP Tracking")
	FVPTrackingFrame TrackingData;

	/** Blend weight for facial morph targets (0.0 = off, 1.0 = full) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VP Tracking", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float FaceBlendWeight = 1.0f;

	/** Apply blendshapes as morph targets on the owning skeletal mesh */
	UFUNCTION(BlueprintCallable, Category = "VP Tracking")
	void ApplyBlendshapesToMorphTargets();

	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
};
