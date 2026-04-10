#include "VPUDPReceiver.h"
#include "Common/UdpSocketBuilder.h"

// Magic header bytes matching Python sender.py
static constexpr uint8 PACKET_MAGIC[] = { 'V', 'P', 'F', 'R' };
static constexpr int32 HEADER_SIZE = 4;      // Magic
static constexpr int32 TIMESTAMP_SIZE = 8;   // double
static constexpr int32 COUNT_SIZE = 2;       // uint16
static constexpr int32 FLOAT_SIZE = 4;       // float

// ARKit 52 blendshape names (fixed order matching MediaPipe output)
static const TArray<FName> BlendshapeNameList = {
	FName("_neutral"),
	FName("browDownLeft"), FName("browDownRight"),
	FName("browInnerUp"),
	FName("browOuterUpLeft"), FName("browOuterUpRight"),
	FName("cheekPuff"),
	FName("cheekSquintLeft"), FName("cheekSquintRight"),
	FName("eyeBlinkLeft"), FName("eyeBlinkRight"),
	FName("eyeLookDownLeft"), FName("eyeLookDownRight"),
	FName("eyeLookInLeft"), FName("eyeLookInRight"),
	FName("eyeLookOutLeft"), FName("eyeLookOutRight"),
	FName("eyeLookUpLeft"), FName("eyeLookUpRight"),
	FName("eyeSquintLeft"), FName("eyeSquintRight"),
	FName("eyeWideLeft"), FName("eyeWideRight"),
	FName("jawForward"), FName("jawLeft"), FName("jawOpen"), FName("jawRight"),
	FName("mouthClose"),
	FName("mouthDimpleLeft"), FName("mouthDimpleRight"),
	FName("mouthFrownLeft"), FName("mouthFrownRight"),
	FName("mouthFunnel"),
	FName("mouthLeft"),
	FName("mouthLowerDownLeft"), FName("mouthLowerDownRight"),
	FName("mouthPressLeft"), FName("mouthPressRight"),
	FName("mouthPucker"),
	FName("mouthRight"),
	FName("mouthRollLower"), FName("mouthRollUpper"),
	FName("mouthShrugLower"), FName("mouthShrugUpper"),
	FName("mouthSmileLeft"), FName("mouthSmileRight"),
	FName("mouthStretchLeft"), FName("mouthStretchRight"),
	FName("mouthUpperUpLeft"), FName("mouthUpperUpRight"),
	FName("noseSneerLeft"), FName("noseSneerRight")
};

UVPUDPReceiver::UVPUDPReceiver()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

const TArray<FName>& UVPUDPReceiver::GetBlendshapeNames()
{
	return BlendshapeNameList;
}

void UVPUDPReceiver::BeginPlay()
{
	Super::BeginPlay();

	// Create UDP socket
	FIPv4Endpoint Endpoint(FIPv4Address(0, 0, 0, 0), ListenPort);

	Socket = FUdpSocketBuilder(TEXT("VPTrackerSocket"))
		.AsNonBlocking()
		.AsReusable()
		.BoundToEndpoint(Endpoint)
		.WithReceiveBufferSize(65536)
		.Build();

	if (!Socket)
	{
		UE_LOG(LogTemp, Error, TEXT("[VPUDPReceiver] Failed to create socket on port %d"), ListenPort);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[VPUDPReceiver] Listening on port %d"), ListenPort);

	// Start async receiver on dedicated thread
	UDPReceiver = new FUdpSocketReceiver(
		Socket,
		FTimespan::FromMilliseconds(1),
		TEXT("VPTrackerRecvThread")
	);

	UDPReceiver->OnDataReceived().BindUObject(this, &UVPUDPReceiver::OnDataReceived);
	UDPReceiver->Start();
}

void UVPUDPReceiver::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UDPReceiver)
	{
		UDPReceiver->Stop();
		delete UDPReceiver;
		UDPReceiver = nullptr;
	}

	if (Socket)
	{
		Socket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
	}

	UE_LOG(LogTemp, Log, TEXT("[VPUDPReceiver] Stopped. Total packets: %d"), PacketCount);
	Super::EndPlay(EndPlayReason);
}

void UVPUDPReceiver::TickComponent(float DeltaTime, ELevelTick TickType,
                                    FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Drain queue on game thread (take latest, discard stale)
	FVPTrackingFrame TempFrame;
	bool bHasNewData = false;

	while (DataQueue.Dequeue(TempFrame))
	{
		LatestFrame = MoveTemp(TempFrame);
		bHasNewData = true;
	}

	if (bHasNewData)
	{
		OnTrackingDataReceived.Broadcast(LatestFrame);

		// Log every 60 packets to confirm reception without flooding
		if (PacketCount % 60 == 0)
		{
			UE_LOG(LogTemp, Log, TEXT("[VPUDPReceiver] Packets=%d BS=%d Pose=%d"),
				PacketCount,
				LatestFrame.FaceData.Blendshapes.Num(),
				LatestFrame.PoseLandmarks.Num());
		}
	}
}

FVPTrackingFrame UVPUDPReceiver::GetLatestTrackingData() const
{
	return LatestFrame;
}

void UVPUDPReceiver::OnDataReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint)
{
	// Called on network thread
	const uint8* RawData = Data->GetData();
	const int32 DataLen = Data->Num();

	if (ParsePacket(RawData, DataLen, ParseBuffer))
	{
		DataQueue.Enqueue(ParseBuffer);
		PacketCount++;
	}
}

bool UVPUDPReceiver::ParsePacket(const uint8* RawData, int32 DataLen, FVPTrackingFrame& OutFrame)
{
	// Minimum: HEADER(4) + TIMESTAMP(8) + BS_COUNT(2) + POSE_COUNT(2) = 16
	if (DataLen < 16)
	{
		return false;
	}

	// Verify magic header
	if (FMemory::Memcmp(RawData, PACKET_MAGIC, HEADER_SIZE) != 0)
	{
		return false;
	}

	int32 Offset = HEADER_SIZE;

	// Timestamp (double, little-endian)
	FMemory::Memcpy(&OutFrame.Timestamp, RawData + Offset, TIMESTAMP_SIZE);
	Offset += TIMESTAMP_SIZE;

	// Blendshape count (uint16, little-endian)
	uint16 NumBS = 0;
	FMemory::Memcpy(&NumBS, RawData + Offset, COUNT_SIZE);
	Offset += COUNT_SIZE;

	// Validate remaining data size for blendshapes
	const int32 BSDataSize = NumBS * FLOAT_SIZE;
	if (Offset + BSDataSize > DataLen)
	{
		return false;
	}

	// Parse blendshapes
	OutFrame.FaceData.Blendshapes.Reset();
	const TArray<FName>& Names = GetBlendshapeNames();
	for (uint16 i = 0; i < NumBS && i < static_cast<uint16>(Names.Num()); ++i)
	{
		float Value = 0.0f;
		FMemory::Memcpy(&Value, RawData + Offset, FLOAT_SIZE);
		Offset += FLOAT_SIZE;
		OutFrame.FaceData.Blendshapes.Add(Names[i], Value);
	}

	// Pose landmark count (uint16)
	if (Offset + COUNT_SIZE > DataLen)
	{
		return false;
	}

	uint16 NumPose = 0;
	FMemory::Memcpy(&NumPose, RawData + Offset, COUNT_SIZE);
	Offset += COUNT_SIZE;

	// Validate remaining data size for pose
	const int32 PoseDataSize = NumPose * 3 * FLOAT_SIZE;  // x, y, z per landmark
	if (Offset + PoseDataSize > DataLen)
	{
		return false;
	}

	// Parse pose landmarks
	OutFrame.PoseLandmarks.SetNum(NumPose);
	for (uint16 i = 0; i < NumPose; ++i)
	{
		float X, Y, Z;
		FMemory::Memcpy(&X, RawData + Offset, FLOAT_SIZE); Offset += FLOAT_SIZE;
		FMemory::Memcpy(&Y, RawData + Offset, FLOAT_SIZE); Offset += FLOAT_SIZE;
		FMemory::Memcpy(&Z, RawData + Offset, FLOAT_SIZE); Offset += FLOAT_SIZE;
		OutFrame.PoseLandmarks[i].Position = FVector(X, Y, Z);
	}

	OutFrame.bIsValid = true;
	return true;
}
