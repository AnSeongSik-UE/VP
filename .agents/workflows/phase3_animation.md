---
description: "Phase 3: 애니메이션 리타겟팅 + 프로파일링 최적화 (Week 4~5)"
---

# Phase 3: 애니메이션 리타겟팅 + 프로파일링 (Week 4~5)

## 3-1. 커스텀 AnimInstance

```cpp
// VPAnimInstance.h - 수신된 트래킹 데이터를 아바타에 매핑
UCLASS()
class UVPAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
    
public:
    // 블렌드쉐이프 → Morph Target 매핑
    UPROPERTY(BlueprintReadWrite, Category="VP Face")
    TMap<FName, float> FaceBlendshapes;
    
    // 포즈 → Bone Transform 매핑
    UPROPERTY(BlueprintReadWrite, Category="VP Pose")
    TArray<FTransform> PoseBoneTransforms;
    
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    
    UFUNCTION(BlueprintCallable, Category="VP Pipeline")
    void ApplyTrackingData(const FVPTrackingFrame& Frame);
};
```

## 3-2. 블렌드쉐이프 → Morph Target 매핑 전략

| ARKit 블렌드쉐이프 | UE Morph Target (예시) | 설명 |
|---------------------|----------------------|------|
| `eyeBlinkLeft` | `EyeBlink_L` | 왼쪽 눈 깜빡임 |
| `eyeBlinkRight` | `EyeBlink_R` | 오른쪽 눈 깜빡임 |
| `mouthSmileLeft` | `MouthSmile_L` | 왼쪽 입꼬리 올림 |
| `jawOpen` | `JawOpen` | 입 벌리기 |
| ... (52개 전체) | ... | 아바타 모델에 맞춰 매핑 |

> **중요:** 아바타 모델의 Morph Target 이름이 ARKit 표준과 다를 수 있음. 매핑 테이블을 DataTable로 관리하여 런타임 교체 가능하게 설계.

## 3-3. Unreal Insights 프로파일링 워크플로우

```bash
# 1. Unreal Insights 트레이싱 활성화 후 에디터 실행
UnrealEditor.exe VPPipeline.uproject -trace=cpu,frame,bookmark

# 2. 트레이싱 데이터 분석
UnrealInsights.exe
```

**프로파일링 체크포인트:**
| 지표 | 목표값 | 초과 시 대응 |
|------|--------|-------------|
| Frame Time | < 16.6ms (60fps) | 드로우콜 감소, LOD 조정 |
| Game Thread | < 8ms | Tick 로직 최적화 |
| Render Thread | < 10ms | 머티리얼 복잡도 감소 |
| GPU Time | < 14ms | Lumen → SSGI 전환 |
| Network Parse (커스텀) | < 0.5ms | 메모리 풀 확인 |

## 3-4. CVar 성능 튜닝 명령어

```
// Lumen 품질 조절
r.Lumen.DiffuseIndirect.Allow 0          // Lumen GI 비활성화
r.Lumen.Reflections.Allow 0               // Lumen 리플렉션 비활성화

// Screen Space 대체
r.ScreenSpaceReflections.Quality 2        // SSGR 품질
r.AmbientOcclusion.Method 1              // SSAO 활성화

// 텍스처 스트리밍
r.Streaming.PoolSize 512                  // 텍스처 풀 512MB 제한
r.Streaming.LimitPoolSizeToVRAM 1         // VRAM 기반 자동 제한

// 렌더링 해상도
r.ScreenPercentage 80                     // 80% 해상도 렌더링 (업스케일)
```

## 3-5. Week 4~5 검증 체크리스트

```
[ ] 아바타 Morph Target에 블렌드쉐이프 실시간 반영 (표정 변화 확인)
[ ] Bone Transform 매핑으로 전신 움직임 반영
[ ] 60fps 안정 유지 (Unreal Insights 확인)
[ ] Game Thread < 8ms
[ ] 메모리 풀링 효과 확인 (GC 스파이크 없음)
[ ] Lumen/SSGI 전환 시 시각 품질 비교
```

**AI 자동화 활용 포인트:**
- `Unreal MCP Server`: "현재 씬의 머티리얼 복잡도 확인해줘" → 에디터 내 조회
- `Claude Desktop`: "ARKit 블렌드쉐이프 52개 전체 목록과 UE Morph Target 매핑 테이블 만들어줘"
- `Antigravity`: AnimInstance 코드 작성 및 리팩터링
