#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Common/UdpSocketReceiver.h"
#include "VPTrackingData.h"
#include "VPUDPReceiver.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTrackingDataReceived, const FVPTrackingFrame&, Data);

/**
 * UDP Receiver Component
 * Listens for binary tracking packets from Python vp-tracker/sender.py
 * Parses the packet on the network thread, delivers to game thread via TQueue
 */
UCLASS(ClassGroup = (VPPipeline), meta = (BlueprintSpawnableComponent))
class VPTRACKERRECEIVER_API UVPUDPReceiver : public UActorComponent
{
	GENERATED_BODY()

public:
	UVPUDPReceiver();

	/** UDP port to listen on (must match sender.py port) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VP Pipeline")
	int32 ListenPort = 7000;

	/** Fired on game thread when new tracking data arrives */
	UPROPERTY(BlueprintAssignable, Category = "VP Pipeline")
	FOnTrackingDataReceived OnTrackingDataReceived;

	/** Get the most recent tracking frame (game thread safe) */
	UFUNCTION(BlueprintCallable, Category = "VP Pipeline")
	FVPTrackingFrame GetLatestTrackingData() const;

	/** Total packets received since BeginPlay */
	UFUNCTION(BlueprintCallable, Category = "VP Pipeline")
	int32 GetPacketCount() const { return PacketCount; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

private:
	FSocket* Socket = nullptr;
	FUdpSocketReceiver* UDPReceiver = nullptr;

	/** Lock-free SPSC queue: network thread -> game thread */
	TQueue<FVPTrackingFrame, EQueueMode::Spsc> DataQueue;

	/** Latest frame on game thread */
	FVPTrackingFrame LatestFrame;

	/** Packet counter */
	int32 PacketCount = 0;

	/** Pre-allocated parse buffer (avoid per-frame allocation) */
	FVPTrackingFrame ParseBuffer;

	/** ARKit blendshape names in the order sent by Python */
	static const TArray<FName>& GetBlendshapeNames();

	/** Called on network thread when UDP data arrives */
	void OnDataReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint);

	/** Parse binary packet into tracking frame */
	bool ParsePacket(const uint8* RawData, int32 DataLen, FVPTrackingFrame& OutFrame);
};
