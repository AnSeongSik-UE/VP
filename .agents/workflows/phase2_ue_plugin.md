---
description: "Phase 2: UE5.7 C++ 수신 플러그인 개발 (Week 2~3)"
---

# Phase 2: UE5.7 C++ 수신 플러그인 (Week 2~3)

## 2-1. C++ 플러그인 프로젝트 구조

```
VPPipeline/
├── Plugins/
│   └── VPTrackerReceiver/
│       ├── VPTrackerReceiver.uplugin
│       ├── Source/
│       │   └── VPTrackerReceiver/
│       │       ├── VPTrackerReceiver.Build.cs
│       │       ├── Public/
│       │       │   ├── VPTrackerReceiver.h        // 모듈 헤더
│       │       │   ├── VPUDPReceiver.h             // UDP 수신 컴포넌트
│       │       │   ├── VPTrackingData.h            // 트래킹 데이터 구조체
│       │       │   └── VPAnimInstance.h             // 커스텀 AnimInstance
│       │       └── Private/
│       │           ├── VPTrackerReceiver.cpp
│       │           ├── VPUDPReceiver.cpp
│       │           ├── VPTrackingData.cpp
│       │           └── VPAnimInstance.cpp
```

## 2-2. 핵심 C++ 구현 가이드

### 트래킹 데이터 구조체 (`VPTrackingData.h`)

```cpp
#pragma once
#include "CoreMinimal.h"
#include "VPTrackingData.generated.h"

USTRUCT(BlueprintType)
struct FVPBlendshapeData
{
    GENERATED_BODY()
    
    // ARKit 52 블렌드쉐이프 가중치 (0.0 ~ 1.0)
    UPROPERTY(BlueprintReadOnly)
    TMap<FName, float> Blendshapes;
};

USTRUCT(BlueprintType)
struct FVPPoseLandmark
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly)
    FVector Position;  // Normalized (0~1)
    
    UPROPERTY(BlueprintReadOnly)
    float Visibility;
};

USTRUCT(BlueprintType)
struct FVPTrackingFrame
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly)
    double Timestamp;
    
    UPROPERTY(BlueprintReadOnly)
    FVPBlendshapeData FaceData;
    
    UPROPERTY(BlueprintReadOnly)
    TArray<FVPPoseLandmark> PoseLandmarks;  // 33개
    
    UPROPERTY(BlueprintReadOnly)
    bool bIsValid = false;
};
```

### UDP 수신 컴포넌트 (`VPUDPReceiver.h`)

```cpp
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Common/UdpSocketReceiver.h"
#include "VPTrackingData.h"
#include "VPUDPReceiver.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTrackingDataReceived, const FVPTrackingFrame&, Data);

UCLASS(ClassGroup=(VPPipeline), meta=(BlueprintSpawnableComponent))
class VPTRACKERRECEIVER_API UVPUDPReceiver : public UActorComponent
{
    GENERATED_BODY()
    
public:
    UVPUDPReceiver();
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VP Pipeline")
    int32 ListenPort = 7000;
    
    UPROPERTY(BlueprintAssignable, Category="VP Pipeline")
    FOnTrackingDataReceived OnTrackingDataReceived;
    
    UFUNCTION(BlueprintCallable, Category="VP Pipeline")
    FVPTrackingFrame GetLatestTrackingData() const;
    
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, 
                               FActorComponentTickFunction* ThisTickFunction) override;

private:
    FSocket* Socket;
    FUdpSocketReceiver* UDPReceiver;
    
    // Thread-safe 데이터 큐
    TQueue<FVPTrackingFrame, EQueueMode::Spsc> DataQueue;
    
    // 게임 스레드에서 사용할 최신 데이터
    FVPTrackingFrame LatestFrame;
    
    // 메모리 풀 (Pre-allocated)
    FVPTrackingFrame ParseBuffer;
    
    void OnDataReceived(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint);
    FVPTrackingFrame ParsePacket(const FArrayReaderPtr& Data);
};
```

## 2-3. Build.cs 의존성

```csharp
// VPTrackerReceiver.Build.cs
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
    "Sockets",
    "Networking",
    "AnimGraphRuntime"
});
```

## 2-4. Week 2~3 검증 체크리스트

```
[ ] 플러그인 컴파일 성공 (UE 에디터 로드)
[ ] UDP 포트 7000 바인딩 성공
[ ] Python 트래커에서 발신한 패킷 수신 확인 (UE_LOG)
[ ] 패킷 파싱 → FVPTrackingFrame 구조체 정상 변환
[ ] TQueue 기반 게임 스레드 전달 → 프레임 드랍 없음 확인
[ ] Unreal Insights에서 네트워크 스레드 CPU 부하 확인
```

**AI 자동화 활용 포인트:**
- `Unreal MCP Server`: Claude에게 "UDP 수신 컴포넌트를 레벨에 배치해줘" → 자연어로 에디터 제어
- `Antigravity`: C++ 플러그인 코드 생성 (USTRUCT, UCLASS 보일러플레이트 자동 생성)
- `MCP check_port_available`: 포트 충돌 사전 진단
- `Claude Desktop`: "FRunnable 워커 스레드 패턴으로 UDP 수신기 코드 작성해줘" → 복잡한 멀티스레딩 코드 AI 생성
