#include "VPTrackerReceiver.h"

#define LOCTEXT_NAMESPACE "FVPTrackerReceiverModule"

void FVPTrackerReceiverModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("[VPTrackerReceiver] Module started"));
}

void FVPTrackerReceiverModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("[VPTrackerReceiver] Module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FVPTrackerReceiverModule, VPTrackerReceiver)
