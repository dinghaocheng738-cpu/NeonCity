#include "Modules/ModuleManager.h"

#include "Editor.h"
#include "EditorViewportClient.h"
#include "Components/ActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "SceneInterface.h"
#include "RenderingThread.h"
#include "UObject/UObjectIterator.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogNeonCityIsolatedAutomation, Log, All);

class FNeonCityArtEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		const TCHAR* CommandLine = FCommandLine::Get();
		if (!FParse::Param(CommandLine, TEXT("NeonCityIsolatedAutomation")))
		{
			return;
		}

		// This is restricted to an explicitly opted-in, unattended, offscreen
		// automation process. No normal editor session is affected.
		if (!FParse::Param(CommandLine, TEXT("unattended")) || !FApp::IsUnattended()
			|| !FParse::Param(CommandLine, TEXT("RenderOffscreen")))
		{
			UE_LOG(LogNeonCityIsolatedAutomation, Warning,
				TEXT("Editor viewport suppression refused: requires -NeonCityIsolatedAutomation -unattended -RenderOffscreen."));
			return;
		}

		bSuppressionActive = true;
		BeginFrameHandle = FCoreDelegates::OnBeginFrame.AddRaw(this, &FNeonCityArtEditorModule::SuppressEditorViewports);
		PreExitHandle = FCoreDelegates::OnEnginePreExit.AddRaw(this, &FNeonCityArtEditorModule::RestoreEditorViewports);
		PostPIEStartedHandle = FEditorDelegates::PostPIEStarted.AddRaw(this, &FNeonCityArtEditorModule::OnPostPIEStarted);
		EndPIEHandle = FEditorDelegates::EndPIE.AddRaw(this, &FNeonCityArtEditorModule::OnEndPIE);
		if (FParse::Param(CommandLine, TEXT("NeonCityGuardedEditorScenePurge")))
		{
			MapReadyHandle = FWorldDelegates::OnPIEMapReady.AddRaw(this, &FNeonCityArtEditorModule::OnPIEMapReady);
			ShutdownPIEHandle = FEditorDelegates::ShutdownPIE.AddRaw(this, &FNeonCityArtEditorModule::OnShutdownPIE);
		}
		UE_LOG(LogNeonCityIsolatedAutomation, Log,
			TEXT("Own-process editor viewport suppression enabled (PID=%u); PIE game rendering, resolution and quality remain unchanged."),
			FPlatformProcess::GetCurrentProcessId());
	}

	virtual void ShutdownModule() override
	{
		RestoreEditorViewports();
	}

private:
	// UE5.8's native editor-scene purge re-registers skinned meshes against a
	// null FScene, which asserts before PIE. Quarantine only the unused editor
	// copies, after PIE duplication, until native RestoreScene has completed.
	void OnPIEMapReady(UGameInstance* GameInstance)
	{
		if (!bSuppressionActive || !GEditor || !GameInstance || !SavedEditorSkinnedComponents.IsEmpty()) return;
		UWorld* PlayWorld = GameInstance->GetWorld();
		UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
		IConsoleVariable* Purge = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PurgeEditorSceneDuringPIE"));
		if (!PlayWorld || PlayWorld->WorldType != EWorldType::PIE || !EditorWorld
			|| EditorWorld->WorldType != EWorldType::Editor || !EditorWorld->Scene
			|| !EditorWorld->Scene->GetSkeletalMeshUpdater() || !Purge) return;
		GuardedEditorWorld = EditorWorld;
		GuardedPlayWorld = PlayWorld;
		OldPurgeValue = Purge->GetInt();
		PlaySkinnedBefore = 0;
		for (TObjectIterator<USkinnedMeshComponent> It; It; ++It)
		{
			if (!IsValid(*It) || !It->IsRegistered()) continue;
			if (It->GetWorld() == PlayWorld) ++PlaySkinnedBefore;
			if (It->GetWorld() == EditorWorld) SavedEditorSkinnedComponents.Add(*It);
		}
		for (const auto& WeakComponent : SavedEditorSkinnedComponents)
			if (USkinnedMeshComponent* Component = WeakComponent.Get()) Component->UnregisterComponent();
		FlushRenderingCommands();
		Purge->Set(1, ECVF_SetByConsole);
		bGuardedPurgeActive = true;
		UE_LOG(LogNeonCityIsolatedAutomation, Log,
			TEXT("Guarded native purge prepared after PIE duplication: editor skinned quarantined=%d PIE registered=%d PIE components touched=0."),
			SavedEditorSkinnedComponents.Num(), PlaySkinnedBefore);
	}

	void OnShutdownPIE(bool)
	{
		RestoreGuardedEditorSkinnedComponents();
	}

	void RestoreGuardedEditorSkinnedComponents()
	{
		if (!bGuardedPurgeActive) return;
		UWorld* EditorWorld = GuardedEditorWorld.Get();
		// EndPIE fires before RestoreScene; only ShutdownPIE or a real-scene
		// cancellation path may re-register these components.
		if (EditorWorld && EditorWorld->Scene && EditorWorld->Scene->GetSkeletalMeshUpdater())
		{
			int32 Restored = 0, Retired = 0;
			for (const auto& WeakComponent : SavedEditorSkinnedComponents)
			{
				USkinnedMeshComponent* Component = WeakComponent.Get();
				if (!IsValid(Component) || Component->GetWorld() != EditorWorld) { ++Retired; continue; }
				if (!Component->IsRegistered()) { Component->RegisterComponent(); ++Restored; }
			}
			FlushRenderingCommands();
			UE_LOG(LogNeonCityIsolatedAutomation, Log,
				TEXT("Guarded native purge editor restoration: registered=%d retired=%d expected=%d real scene=true."),
				Restored, Retired, SavedEditorSkinnedComponents.Num());
		}
		else
		{
			UE_LOG(LogNeonCityIsolatedAutomation, Warning,
				TEXT("Guarded native purge has no restored real editor scene; components remain quarantined for process teardown."));
		}
		if (IConsoleVariable* Purge = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PurgeEditorSceneDuringPIE"))) Purge->Set(OldPurgeValue, ECVF_SetByConsole);
		bGuardedPurgeActive = false;
		SavedEditorSkinnedComponents.Reset(); GuardedEditorWorld.Reset(); GuardedPlayWorld.Reset();
	}

	struct FSavedEditorViewport
	{
		FEditorViewportClient* Client = nullptr;
		FViewportStateGetter OriginalVisibility;
		FDelegateHandle SuppressedVisibilityHandle;
		FText RealtimeOverrideName;
	};

	void SuppressEditorViewports()
	{
		if (!bSuppressionActive || !GEditor)
		{
			return;
		}
		SuppressEditorRenderStates();

		// These are editor clients only. The native UGameViewportClient used by
		// the separate PIE window is not in this list and is never modified.
		const TArray<FEditorViewportClient*> LiveClients = GEditor->GetAllViewportClients();
		SavedViewports.RemoveAll([&LiveClients](const FSavedEditorViewport& Saved)
		{
			// Editor clients are not UObjects: never dereference retired pointers.
			return !LiveClients.Contains(Saved.Client);
		});

		for (FEditorViewportClient* Client : LiveClients)
		{
			if (!Client)
			{
				continue;
			}

			FSavedEditorViewport* Saved = SavedViewports.FindByPredicate([Client](const FSavedEditorViewport& Candidate)
			{
				return Candidate.Client == Client;
			});

			// Preserve an editor client if it is being used for SIE/PIE itself.
			// Our automated evidence uses a separate normal PIE game viewport.
			if (const UWorld* World = Client->GetWorld(); World && World->WorldType == EWorldType::PIE)
			{
				if (Saved)
				{
					RestoreLiveClient(*Saved);
					SavedViewports.RemoveAll([Client](const FSavedEditorViewport& Candidate) { return Candidate.Client == Client; });
				}
				continue;
			}

			if (!Saved)
			{
				FSavedEditorViewport& NewSaved = SavedViewports.AddDefaulted_GetRef();
				NewSaved.Client = Client;
				NewSaved.RealtimeOverrideName = FText::FromString(FString::Printf(
					TEXT("NeonCity isolated automation %u viewport %u"), FPlatformProcess::GetCurrentProcessId(), ++ViewportSerial));
				Saved = &NewSaved;
			}

			if (!Saved->SuppressedVisibilityHandle.IsValid()
				|| Client->VisibilityDelegate.GetHandle() != Saved->SuppressedVisibilityHandle)
			{
				// Capture a newly created/rebound delegate, including tab layouts
				// rebuilt during map loading. Its unique handle also prevents a
				// recycled raw client address from receiving an old restoration.
				Saved->OriginalVisibility = Client->VisibilityDelegate;
				Client->VisibilityDelegate.BindLambda([]() { return false; });
				Saved->SuppressedVisibilityHandle = Client->VisibilityDelegate.GetHandle();
			}

			if (!Client->HasRealtimeOverride(Saved->RealtimeOverrideName))
			{
				// SetRealtime is persisted to editor settings; use a temporary,
				// individually named override instead. Visibility additionally
				// prevents non-realtime invalidation redraws in EditorEngine.
				Client->AddRealtimeOverride(false, Saved->RealtimeOverrideName);
			}
		}

		if (LastReportedViewportCount != SavedViewports.Num())
		{
			LastReportedViewportCount = SavedViewports.Num();
			UE_LOG(LogNeonCityIsolatedAutomation, Log,
				TEXT("Editor-only viewport visibility suppressed: %d live clients; cached Slate viewport textures retained."),
				LastReportedViewportCount);
		}
	}

	static void RestoreLiveClient(const FSavedEditorViewport& Saved)
	{
		// Called only after membership in GEditor's current client list has
		// been established. Never replace a delegate rebound by another owner.
		if (Saved.Client->VisibilityDelegate.GetHandle() == Saved.SuppressedVisibilityHandle)
		{
			Saved.Client->VisibilityDelegate = Saved.OriginalVisibility;
		}
		if (Saved.Client->HasRealtimeOverride(Saved.RealtimeOverrideName))
		{
			Saved.Client->RemoveRealtimeOverride(Saved.RealtimeOverrideName, false);
		}
	}

	void OnPostPIEStarted(bool bSimulating)
	{
		if (bGuardedPurgeActive)
		{
			int32 PlayRegistered = 0, EditorRegistered = 0;
			for (TObjectIterator<USkinnedMeshComponent> It; It; ++It)
			{
				if (!IsValid(*It) || !It->IsRegistered()) continue;
				if (It->GetWorld() == GuardedPlayWorld.Get()) ++PlayRegistered;
				if (It->GetWorld() == GuardedEditorWorld.Get()) ++EditorRegistered;
			}
			UE_LOG(LogNeonCityIsolatedAutomation, Log,
				TEXT("Guarded native purge PIE started: PIE registered=%d before=%d editor registered=%d; game quality and input unchanged."),
				PlayRegistered, PlaySkinnedBefore, EditorRegistered);
			ensureAlwaysMsgf(PlayRegistered >= PlaySkinnedBefore && EditorRegistered == 0, TEXT("Guarded purge component isolation failed"));
		}
		// Retained Land15-Guard04 failed with a D3D12 GPU page fault before
		// gameplay evidence. This experiment is rejected for acceptance runs;
		// keep editor primitive render states unless separately investigating it.
		if (!FParse::Param(FCommandLine::Get(), TEXT("NeonCityExperimentalEditorPrimitiveSuppression")))
		{
			return;
		}
		if (!bSuppressionActive || bSimulating || !GEditor || !GEditor->PlayWorld.Get())
		{
			return;
		}
		RestoreEditorRenderStates();
		UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
		if (!IsValid(EditorWorld) || EditorWorld->WorldType != EWorldType::Editor || !EditorWorld->Scene)
		{
			UE_LOG(LogNeonCityIsolatedAutomation, Warning, TEXT("Editor render-state suppression skipped: no live editor world with a real scene."));
			return;
		}
		SuppressedEditorWorld = EditorWorld;
		bEditorRenderStatesSuppressed = true;
		SuppressEditorRenderStates();
	}

	void OnEndPIE(bool)
	{
		RestoreEditorRenderStates();
	}

	void SuppressEditorRenderStates()
	{
		if (!bEditorRenderStatesSuppressed || !GEditor || !GEditor->PlayWorld.Get())
		{
			return;
		}
		UWorld* EditorWorld = SuppressedEditorWorld.Get();
		if (!EditorWorld || EditorWorld->WorldType != EWorldType::Editor || !EditorWorld->Scene
			|| GEditor->GetEditorWorldContext().World() != EditorWorld)
		{
			RestoreEditorRenderStates();
			return;
		}

		int32 RemovedThisBatch = 0;
		for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
		{
			UPrimitiveComponent* Component = *It;
			// Never unregister components, alter physics, replace any FScene, or
			// operate on PIE/preview worlds. Weak references permit normal GC.
			if (!IsValid(Component) || Component->GetWorld() != EditorWorld
				|| !Component->IsRegistered() || !Component->IsRenderStateCreated())
			{
				continue;
			}
			SavedEditorRenderComponents.Add(Component);
			// UE's public primitive interface is the native, friend-authorized
			// bridge to render-state lifecycle methods. It removes only the
			// primitive's scene proxy: component registration and physics stay
			// intact. Keep no interface pointer across frames or GC. Non-primitive
			// editor lights/sky retain their states rather than retaining a raw
			// FComponentRecreateRenderStateContext across a latent PIE session.
			Component->GetPrimitiveComponentInterface()->DestroyRenderState();
			++RemovedThisBatch;
		}
		if (RemovedThisBatch)
		{
			// A hidden editor scene will not draw and drain pending primitive
			// removals itself. Use the same native batch update as render-state
			// contexts, restricted to this one retained real editor FScene.
			TSet<FSceneInterface*> Scenes;
			Scenes.Add(EditorWorld->Scene);
			UpdateAllPrimitiveSceneInfosForScenes(MoveTemp(Scenes));
			FlushRenderingCommands();
			UE_LOG(LogNeonCityIsolatedAutomation, Log,
				TEXT("Own editor-world primitive render states removed: batch=%d tracked=%d world=%s; real scene and non-primitive light/sky states retained, PIE components touched=0, no physics/quality changes."),
				RemovedThisBatch, SavedEditorRenderComponents.Num(), *EditorWorld->GetName());
		}
	}

	void RestoreEditorRenderStates()
	{
		bEditorRenderStatesSuppressed = false;
		UWorld* EditorWorld = SuppressedEditorWorld.Get();
		int32 Restored = 0;
		if (EditorWorld && EditorWorld->WorldType == EWorldType::Editor && EditorWorld->Scene)
		{
			for (const TWeakObjectPtr<UPrimitiveComponent>& WeakComponent : SavedEditorRenderComponents)
			{
				UPrimitiveComponent* Component = WeakComponent.Get();
				if (Component && Component->GetWorld() == EditorWorld && Component->IsRegistered() && !Component->IsRenderStateCreated())
				{
					// Reacquire the public bridge only after the weak component and
					// retained editor world have passed the restoration guards.
					Component->GetPrimitiveComponentInterface()->CreateRenderState(nullptr);
					++Restored;
				}
			}
			if (Restored)
			{
				TSet<FSceneInterface*> Scenes;
				Scenes.Add(EditorWorld->Scene);
				UpdateAllPrimitiveSceneInfosForScenes(MoveTemp(Scenes));
				FlushRenderingCommands();
			}
		}
		if (SavedEditorRenderComponents.Num())
		{
			UE_LOG(LogNeonCityIsolatedAutomation, Log, TEXT("Own editor-world primitive render states restored: %d; retired weak components skipped."), Restored);
		}
		SavedEditorRenderComponents.Reset();
		SuppressedEditorWorld.Reset();
	}

	void RestoreEditorViewports()
	{
		if (!bSuppressionActive)
		{
			return;
		}
		bSuppressionActive = false;
		FCoreDelegates::OnBeginFrame.Remove(BeginFrameHandle);
		FCoreDelegates::OnEnginePreExit.Remove(PreExitHandle);
		FEditorDelegates::PostPIEStarted.Remove(PostPIEStartedHandle);
		FEditorDelegates::EndPIE.Remove(EndPIEHandle);
		FWorldDelegates::OnPIEMapReady.Remove(MapReadyHandle);
		FEditorDelegates::ShutdownPIE.Remove(ShutdownPIEHandle);
		BeginFrameHandle.Reset();
		PreExitHandle.Reset();
		PostPIEStartedHandle.Reset();
		EndPIEHandle.Reset();
		MapReadyHandle.Reset(); ShutdownPIEHandle.Reset();
		RestoreGuardedEditorSkinnedComponents();
		RestoreEditorRenderStates();

		if (GEditor)
		{
			const TArray<FEditorViewportClient*> LiveClients = GEditor->GetAllViewportClients();
			for (const FSavedEditorViewport& Saved : SavedViewports)
			{
				if (LiveClients.Contains(Saved.Client))
				{
					RestoreLiveClient(Saved);
				}
			}
		}
		SavedViewports.Reset();
		UE_LOG(LogNeonCityIsolatedAutomation, Log, TEXT("Own-process editor viewport suppression restored."));
	}

	bool bSuppressionActive = false;
	uint32 ViewportSerial = 0;
	int32 LastReportedViewportCount = INDEX_NONE;
	FDelegateHandle BeginFrameHandle;
	FDelegateHandle PreExitHandle;
	FDelegateHandle PostPIEStartedHandle;
	FDelegateHandle EndPIEHandle;
	FDelegateHandle MapReadyHandle, ShutdownPIEHandle;
	bool bGuardedPurgeActive = false;
	int32 OldPurgeValue = 0, PlaySkinnedBefore = 0;
	TWeakObjectPtr<UWorld> GuardedEditorWorld, GuardedPlayWorld;
	TArray<TWeakObjectPtr<USkinnedMeshComponent>> SavedEditorSkinnedComponents;
	bool bEditorRenderStatesSuppressed = false;
	TWeakObjectPtr<UWorld> SuppressedEditorWorld;
	TSet<TWeakObjectPtr<UPrimitiveComponent>> SavedEditorRenderComponents;
	TArray<FSavedEditorViewport> SavedViewports;
};

IMPLEMENT_MODULE(FNeonCityArtEditorModule, NeonCityArtEditor)
