#include "LuaScriptComponent.h"
#include "Runtime/Engine/Classes/GameFramework/Actor.h"
#include "LuaBindingHelpers.h"
#include "World/World.h"
#include "LuaScriptFileUtils.h"
#include <tchar.h>
#include "Engine/EditorEngine.h"

ULuaScriptComponent::ULuaScriptComponent()
{
}

ULuaScriptComponent::~ULuaScriptComponent()
{
}

void ULuaScriptComponent::BeginPlay()
{
    Super::BeginPlay();

    KeyDownDelegateHandles.Empty();
    
    FSlateAppMessageHandler* Handler = GEngineLoop.GetAppMessageHandler();
    
    CallLuaFunction("BeginPlay");
}

void ULuaScriptComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{ 
    Super::EndPlay(EndPlayReason);
    
    CallLuaFunction("EndPlay");
}

UObject* ULuaScriptComponent::Duplicate(UObject* InOuter)
{
    ULuaScriptComponent* NewComponent = Cast<ULuaScriptComponent>(Super::Duplicate(InOuter));
    if (NewComponent)
    {
        NewComponent->ScriptPath = ScriptPath;
        NewComponent->DisplayName = DisplayName;
    }
    return NewComponent;
}

/* ActorComponent가 Actor와 World에 등록이 되었다는 전제하에 호출됩니다
 * So That we can use GetOwner() and GetWorld() safely
 */
void ULuaScriptComponent::InitializeComponent()
{
    Super::InitializeComponent();

    InitializeLuaState();
}

void ULuaScriptComponent::SetScriptPath(const FString& InScriptPath)
{
    ScriptPath = InScriptPath;
    bScriptValid = false;
}

void ULuaScriptComponent::InitializeLuaState()
{
    if (ScriptPath.IsEmpty()) {
        bool bSuccess = LuaScriptFileUtils::CopyTemplateToActorScript(
            L"template.lua",
            GetOwner()->GetWorld()->GetName().ToWideString(),
            GetOwner()->GetName().ToWideString(),
            ScriptPath,
            DisplayName
        );
        if (!bSuccess) {
            UE_LOG(LogLevel::Error, TEXT("Failed to create script from template"));
            return;
        }
    }

    LuaState.open_libraries();
    BindEngineAPI();

    try {
        LuaState.script_file((*ScriptPath));
        bScriptValid = true;
        const std::wstring FilePath = ScriptPath.ToWideString();
        LastWriteTime = std::filesystem::last_write_time(FilePath);
    }
    catch (const sol::error& err) {
        UE_LOG(LogLevel::Error, TEXT("Lua Initialization error: %s"), err.what());
    }
}

void ULuaScriptComponent::BindEngineAPI()
{
    // [1] 바인딩 전 글로벌 키 스냅샷
    TArray<FString> Before = LuaDebugHelper::CaptureGlobalNames(LuaState);

    LuaBindingHelpers::BindPrint(LuaState);    // 0) Print 바인딩
    LuaBindingHelpers::BindFVector(LuaState);   // 2) FVector 바인딩

    auto ActorType = LuaState.new_usertype<AActor>("Actor",
        sol::constructors<>(),
        "Location", sol::property(
            &AActor::GetActorLocation,
            &AActor::SetActorLocation
        )
    );
    
    // 프로퍼티 바인딩
    LuaState["actor"] = GetOwner();

    // [2] 바인딩 후, 새로 추가된 글로벌 키만 자동 로그
    LuaDebugHelper::LogNewBindings(LuaState, Before);
}


bool ULuaScriptComponent::CheckFileModified()
{
    if (ScriptPath.IsEmpty()) return false;

    try {
        std::wstring FilePath = ScriptPath.ToWideString();
        const auto CurrentTime = std::filesystem::last_write_time(FilePath);

        if (CurrentTime > LastWriteTime) {
            LastWriteTime = CurrentTime;
            return true;
        }
    }
    catch (const std::exception& e) {
        UE_LOG(LogLevel::Error, TEXT("Failed to check lua script file"));
    }
    return false;
}

void ULuaScriptComponent::ReloadScript()
{
    sol::table PersistentData;
    if (bScriptValid && LuaState["PersistentData"].valid()) {
        PersistentData = LuaState["PersistentData"];
    }

    LuaState = sol::state();
    InitializeLuaState();

    if (PersistentData.valid()) {
        LuaState["PersistentData"] = PersistentData;
    }

    CallLuaFunction("OnHotReload");
    CallLuaFunction("BeginPlay");
}

void ULuaScriptComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    CallLuaFunction("Tick", DeltaTime);

    if (CheckFileModified()) {
        try {
            ReloadScript();
            UE_LOG(LogLevel::Display, TEXT("Lua script reloaded"));
        }
        catch (const sol::error& e) {
            UE_LOG(LogLevel::Error, TEXT("Failed to reload lua script"));
        }
    }
}

