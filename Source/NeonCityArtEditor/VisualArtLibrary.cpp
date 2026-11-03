#include "VisualArtLibrary.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_LinkedAnimGraph.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_TwoWayBlend.h"
#include "AnimGraphNode_SequenceEvaluator.h"
#include "AnimGraphNode_BlendListByInt.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "Presentation/CharacterPresentationAnimInstance.h"
#include "Presentation/TransportOccupantAnimInstance.h"
#include "AnimGraphNode_TwoBoneIK.h"
#include "AnimGraphNode_ModifyBone.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Water/SwimmingAnimInstance.h"

namespace
{
template<class T> T* Add(UEdGraph* Graph)
{
    FGraphNodeCreator<T> Creator(*Graph);
    T* Node=Creator.CreateNode(); Creator.Finalize(); return Node;
}
bool Wire(UEdGraph* Graph,UEdGraphNode* From,FName Output,UEdGraphNode* To,FName Input)
{
    auto* A=From?From->FindPin(Output,EGPD_Output):nullptr;
    auto* B=To?To->FindPin(Input,EGPD_Input):nullptr;
    if(!A || !B) { UE_LOG(LogTemp,Error,TEXT("Art graph missing pin %s -> %s"),*Output.ToString(),*Input.ToString()); return false; }
    return Graph->GetSchema()->TryCreateConnection(A,B);
}
UEdGraph* FindGraph(UAnimBlueprint* BP)
{
    for(UEdGraph* G:BP->FunctionGraphs) if(G && G->GetFName()==TEXT("AnimGraph")) return G;
    return nullptr;
}
}
FString UVisualArtLibrary::DescribeAnimationGraph(UAnimBlueprint* BP)
{
    if(!BP) return TEXT("Missing blueprint");
    TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs); FString Result;
    for(auto* Graph:Graphs)
        for(UEdGraphNode* Node:Graph->Nodes)
            Result+=Graph->GetName()+TEXT(" | ")+Node->GetClass()->GetName()+TEXT(" | ")+Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString()+TEXT("\n");
    return Result;
}
TArray<FVector> UVisualArtLibrary::GetStaticMeshVertices(UStaticMesh* Mesh)
{
 TArray<FVector> Result;
 const auto* Data=Mesh?Mesh->GetRenderData():nullptr;
 if(!Data || Data->LODResources.IsEmpty())return Result;
 const auto& Positions=Data->LODResources[0].VertexBuffers.PositionVertexBuffer;
 for(uint32 I=0;I<Positions.GetNumVertices();++I)Result.Add(FVector(Positions.VertexPosition(I)));
 return Result;
}
bool UVisualArtLibrary::BuildDiveHandsGraph(UAnimBlueprint* BP)
{
 if(!BP || !BP->GetPathName().StartsWith(TEXT("/Game/NeonCity/Art/OceanExperience/Equipment/R2/Hands/ABP_DiveHands")))return false;
 auto* G=FindGraph(BP);if(!G)return false;
 UAnimGraphNode_Root* Root=nullptr;
 auto Nodes=G->Nodes;for(UEdGraphNode* N:Nodes)if(auto* R=Cast<UAnimGraphNode_Root>(N))Root=R;else G->RemoveNode(N);
 if(!Root)return false;BP->Modify();G->Modify();bool OK=true;
 auto Bind=[&](const TCHAR* Name,UEdGraphNode* To,const TCHAR* Pin)
 {
  auto* V=Add<UK2Node_VariableGet>(G);V->VariableReference.SetSelfMember(Name);V->ReconstructNode();OK=Wire(G,V,Name,To,Pin) && OK;
 };
 auto* Choice=Add<UAnimGraphNode_BlendListByInt>(G);
 const TCHAR* Names[]={TEXT("Hold"),TEXT("Aim"),TEXT("Fire"),TEXT("Recover"),TEXT("DPV")};
 while(Choice->Node.GetBlendTimes().Num()<5)Choice->AddPinToBlendList();
 for(int32 I=0;I<5;++I)
 {
  auto* Clip=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/NeonCity/Art/OceanExperience/Equipment/R2/Hands/A_FPDive_%s"),Names[I]));if(!Clip)return false;
  UEdGraphNode* Node=nullptr;
  if(I==2 || I==3)
  {
   FGraphNodeCreator<UAnimGraphNode_SequenceEvaluator> C(*G);auto* N=C.CreateNode();N->Node.SetSequence(Clip);C.Finalize();
   for(auto& P:N->ShowPinForProperties)if(P.PropertyName==TEXT("ExplicitTime"))P.bShowPin=true;
   N->ReconstructNode();Bind(TEXT("ActionTime"),N,TEXT("ExplicitTime"));Node=N;
  }
  else
  {
   FGraphNodeCreator<UAnimGraphNode_SequencePlayer> C(*G);auto* N=C.CreateNode();N->Node.SetSequence(Clip);N->Node.SetLoopAnimation(true);C.Finalize();Node=N;
  }
  OK=Wire(G,Node,TEXT("Pose"),Choice,*FString::Printf(TEXT("BlendPose_%d"),I)) && OK;
  if(auto* T=Choice->FindPin(*FString::Printf(TEXT("BlendTime_%d"),I)))T->DefaultValue=I==2?TEXT("0.06"):TEXT("0.22");
 }
 Bind(TEXT("HandPose"),Choice,TEXT("ActiveChildIndex"));OK=Wire(G,Choice,TEXT("Pose"),Root,TEXT("Result")) && OK;
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);return OK && BP->Status!=BS_Error;
}
bool UVisualArtLibrary::AddDivingPresentation(UAnimBlueprint* BP)
{
 auto* G=BP?FindGraph(BP):nullptr;if(!G)return false;
 int32 Existing=0;for(UEdGraphNode* N:G->Nodes)if(N->NodeComment==TEXT("OceanExperienceDivePose"))++Existing;
 if(Existing==3)return AddOceanLocomotionR2(BP);
 if(Existing)return false;
 TArray<UAnimGraphNode_SequencePlayer*> Original;TArray<UAnimSequence*> Clips;
 for(const TCHAR* Name:{TEXT("Underwater"),TEXT("Ascend"),TEXT("Descend")})
 {
  UAnimGraphNode_SequencePlayer* Found=nullptr;
  for(UEdGraphNode* N:G->Nodes)if(auto* P=Cast<UAnimGraphNode_SequencePlayer>(N))
   if(P->Node.GetSequence() && P->Node.GetSequence()->GetName()==FString(TEXT("AN_NC_Swim_"))+Name)Found=P;
  auto* Clip=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/NeonCity/Art/OceanExperience/Animations/A_Dive_%s"),Name));
  if(!Found || !Clip || !Found->FindPin(TEXT("Pose"))->LinkedTo.Num())return false;
  Original.Add(Found);Clips.Add(Clip);
 }
 BP->Modify();G->Modify();bool OK=true;
 for(int32 I=0;I<3;++I)
 {
  auto* Old=Original[I];auto* Pose=Old->FindPin(TEXT("Pose"));auto Targets=Pose->LinkedTo;
  FGraphNodeCreator<UAnimGraphNode_SequencePlayer> PC(*G);auto* New=PC.CreateNode();New->Node.SetSequence(Clips[I]);New->Node.SetLoopAnimation(true);PC.Finalize();
  New->NodePosX=Old->NodePosX;New->NodePosY=Old->NodePosY+90;
  auto* Blend=Add<UAnimGraphNode_TwoWayBlend>(G);Blend->NodeComment=TEXT("OceanExperienceDivePose");Blend->NodePosX=Old->NodePosX+240;Blend->NodePosY=Old->NodePosY;
  FGraphNodeCreator<UK2Node_VariableGet> VC(*G);auto* Alpha=VC.CreateNode();Alpha->VariableReference.SetSelfMember(TEXT("DiveAlpha"));VC.Finalize();
  Alpha->NodePosX=Old->NodePosX;Alpha->NodePosY=Old->NodePosY+180;
  Pose->BreakAllPinLinks();OK=Wire(G,Old,TEXT("Pose"),Blend,TEXT("A")) && OK;
  OK=Wire(G,New,TEXT("Pose"),Blend,TEXT("B")) && OK;OK=Wire(G,Alpha,TEXT("DiveAlpha"),Blend,TEXT("Alpha")) && OK;
  for(auto* Target:Targets)OK=G->GetSchema()->TryCreateConnection(Blend->FindPin(TEXT("Pose")),Target) && OK;
 }
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
 return OK && BP->Status!=BS_Error && AddOceanLocomotionR2(BP);
}
bool UVisualArtLibrary::CorrectOceanLocomotionR3(UAnimBlueprint* BP)
{
 auto* G=BP?FindGraph(BP):nullptr;if(!G)return false;
 UAnimGraphNode_BlendListByInt* Choice=nullptr;
 for(UEdGraphNode* N:G->Nodes)if(N->NodeComment==TEXT("OceanExperienceLocomotionR2"))
 {
  auto* B=N->FindPin(TEXT("B"));
  Choice=B && B->LinkedTo.Num()==1?Cast<UAnimGraphNode_BlendListByInt>(B->LinkedTo[0]->GetOwningNode()):nullptr;
 }
 if(!Choice || (Choice->Node.GetBlendTimes().Num()!=19 && Choice->Node.GetBlendTimes().Num()!=22))return false;
 const TCHAR* Names[]={TEXT("Tread"),TEXT("SurfaceForward"),TEXT("Slow"),TEXT("Forward"),TEXT("Fast"),TEXT("TurnLeft"),TEXT("TurnRight"),TEXT("Ascend"),TEXT("Descend"),TEXT("FloorStand"),TEXT("FloorWalk"),TEXT("PushOff"),TEXT("ToolHold"),TEXT("SpearAim"),TEXT("SpearUse"),TEXT("Recover"),TEXT("DPV"),TEXT("FloorDPVStand"),TEXT("FloorDPVWalk"),TEXT("Neutral"),TEXT("FloorSpearStand"),TEXT("FloorSpearWalk")};
 TArray<UAnimSequence*> Clips;
 for(const auto* Name:Names)
 {
  auto* Clip=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/NeonCity/Art/OceanExperience/Animations/R3/A_OceanR3_%s"),Name));
  if(!Clip)return false;Clips.Add(Clip);
 }
 BP->Modify();G->Modify();bool OK=true;
 while(Choice->Node.GetBlendTimes().Num()<Clips.Num())Choice->AddPinToBlendList();
 for(int32 I=0;I<Clips.Num();++I)
 {
  auto* Pin=Choice->FindPin(*FString::Printf(TEXT("BlendPose_%d"),I));if(!Pin)return false;
  if(Pin->LinkedTo.Num()==1)
  {
   auto* N=Pin->LinkedTo[0]->GetOwningNode();
   if(auto* Player=Cast<UAnimGraphNode_SequencePlayer>(N))Player->Node.SetSequence(Clips[I]);
   else if(auto* Eval=Cast<UAnimGraphNode_SequenceEvaluator>(N))Eval->Node.SetSequence(Clips[I]);
   else return false;
  }
  else if(Pin->LinkedTo.IsEmpty() && I>=19)
  {
   FGraphNodeCreator<UAnimGraphNode_SequencePlayer> C(*G);auto* Player=C.CreateNode();Player->Node.SetSequence(Clips[I]);Player->Node.SetLoopAnimation(true);C.Finalize();
   for(auto& P:Player->ShowPinForProperties)if(P.PropertyName==TEXT("PlayRate"))P.bShowPin=true;
   Player->ReconstructNode();Player->NodePosX=200;Player->NodePosY=1200+I*200;
   FGraphNodeCreator<UK2Node_VariableGet> V(*G);auto* Rate=V.CreateNode();Rate->VariableReference.SetSelfMember(TEXT("OceanPoseRate"));V.Finalize();
   OK=Wire(G,Rate,TEXT("OceanPoseRate"),Player,TEXT("PlayRate")) && OK;
   OK=Wire(G,Player,TEXT("Pose"),Choice,*FString::Printf(TEXT("BlendPose_%d"),I)) && OK;
  }
  else return false;
  if(auto* Time=Choice->FindPin(*FString::Printf(TEXT("BlendTime_%d"),I)))Time->DefaultValue=I==11?TEXT("0.3"):TEXT("0.55");
 }
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);return OK && BP->Status!=BS_Error;
}

bool UVisualArtLibrary::AddOceanLocomotionR2(UAnimBlueprint* BP)
{
 auto* G=BP?FindGraph(BP):nullptr;if(!G)return false;
 for(UEdGraphNode* N:G->Nodes)if(N->NodeComment==TEXT("OceanExperienceLocomotionR2"))
 {
  // Migrate only the existing ocean selector. Preserve every authored graph
  // connection, including the land/combat/fishing presentation downstream.
  auto* B=N->FindPin(TEXT("B"));
  auto* Choice=B && B->LinkedTo.Num()==1?Cast<UAnimGraphNode_BlendListByInt>(B->LinkedTo[0]->GetOwningNode()):nullptr;
  if(!Choice)return false;
  const int32 Count=Choice->Node.GetBlendTimes().Num();if(Count==19)return true;if(Count!=17)return false;
  TArray<UAnimSequence*> Extra;
  for(const auto* Name:{TEXT("FloorDPVStand"),TEXT("FloorDPVWalk")})
  {auto* C=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/NeonCity/Art/OceanExperience/Animations/R2/A_OceanR2_%s"),Name));if(!C)return false;Extra.Add(C);}
  BP->Modify();G->Modify();bool OK=true;
  for(int32 I=0;I<Extra.Num();++I)
  {
   Choice->AddPinToBlendList();
   FGraphNodeCreator<UAnimGraphNode_SequencePlayer> C(*G);auto* Player=C.CreateNode();Player->Node.SetSequence(Extra[I]);Player->Node.SetLoopAnimation(true);C.Finalize();
   for(auto& P:Player->ShowPinForProperties)if(P.PropertyName==TEXT("PlayRate"))P.bShowPin=true;
   Player->ReconstructNode();Player->NodePosX=200;Player->NodePosY=4600+I*200;
   FGraphNodeCreator<UK2Node_VariableGet> V(*G);auto* Rate=V.CreateNode();Rate->VariableReference.SetSelfMember(TEXT("OceanPoseRate"));V.Finalize();
   OK=Wire(G,Rate,TEXT("OceanPoseRate"),Player,TEXT("PlayRate")) && OK;
   OK=Wire(G,Player,TEXT("Pose"),Choice,*FString::Printf(TEXT("BlendPose_%d"),17+I)) && OK;
   if(auto* Time=Choice->FindPin(*FString::Printf(TEXT("BlendTime_%d"),17+I)))Time->DefaultValue=TEXT("0.32");
  }
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);return OK && BP->Status!=BS_Error;
 }
 // Extend the existing water/land blend before combat, flashlight and hand IK.
 // No existing authored graph is destroyed or reconstructed.
 UAnimGraphNode_TwoWayBlend* WaterBlend=nullptr;
 for(UEdGraphNode* N:G->Nodes)if(auto* B=Cast<UAnimGraphNode_TwoWayBlend>(N))
  if(auto* Alpha=B->FindPin(TEXT("Alpha")))for(auto* Link:Alpha->LinkedTo)
   if(Link->PinName==TEXT("WaterAlpha"))WaterBlend=B;
 if(!WaterBlend)return false;
 const TCHAR* Names[]={TEXT("Tread"),TEXT("SurfaceForward"),TEXT("Slow"),TEXT("Forward"),TEXT("Fast"),TEXT("TurnLeft"),TEXT("TurnRight"),TEXT("Ascend"),TEXT("Descend"),TEXT("FloorStand"),TEXT("FloorWalk"),TEXT("PushOff"),TEXT("ToolHold"),TEXT("SpearAim"),TEXT("SpearUse"),TEXT("Recover"),TEXT("DPV"),TEXT("FloorDPVStand"),TEXT("FloorDPVWalk")};
 TArray<UAnimSequence*> Clips;
 for(const auto* Name:Names)
 {
  auto* Clip=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/NeonCity/Art/OceanExperience/Animations/R2/A_OceanR2_%s"),Name));
  if(!Clip)return false;Clips.Add(Clip);
 }
 auto* Pose=WaterBlend->FindPin(TEXT("Pose"));if(!Pose || Pose->LinkedTo.IsEmpty())return false;
 BP->Modify();G->Modify();bool OK=true;
 auto Bind=[&](const TCHAR* Name,UEdGraphNode* To,const TCHAR* Pin)
 {
  FGraphNodeCreator<UK2Node_VariableGet> VC(*G);auto* V=VC.CreateNode();V->VariableReference.SetSelfMember(Name);VC.Finalize();
  V->NodePosX=To->NodePosX-220;V->NodePosY=To->NodePosY+110;
  OK=Wire(G,V,Name,To,Pin) && OK;
 };
 auto* Choice=Add<UAnimGraphNode_BlendListByInt>(G);
 Choice->NodePosX=850;Choice->NodePosY=1500;
 // A newly placed BlendListByInt owns two entries.
 while(Choice->Node.GetBlendTimes().Num()<Clips.Num())Choice->AddPinToBlendList();
 for(int32 I=0;I<Clips.Num();++I)
 {
  UEdGraphNode* ClipNode=nullptr;
  if(I==11 || I==14 || I==15)
  {
   FGraphNodeCreator<UAnimGraphNode_SequenceEvaluator> C(*G);auto* N=C.CreateNode();N->Node.SetSequence(Clips[I]);C.Finalize();ClipNode=N;
   for(auto& Pin:N->ShowPinForProperties)if(Pin.PropertyName==TEXT("ExplicitTime"))Pin.bShowPin=true;
   N->ReconstructNode();Bind(TEXT("OceanActionTime"),N,TEXT("ExplicitTime"));
  }
  else
  {
   FGraphNodeCreator<UAnimGraphNode_SequencePlayer> C(*G);auto* N=C.CreateNode();N->Node.SetSequence(Clips[I]);N->Node.SetLoopAnimation(true);C.Finalize();ClipNode=N;
   for(auto& Pin:N->ShowPinForProperties)if(Pin.PropertyName==TEXT("PlayRate"))Pin.bShowPin=true;
   N->ReconstructNode();Bind(TEXT("OceanPoseRate"),N,TEXT("PlayRate"));
  }
  ClipNode->NodePosX=200;ClipNode->NodePosY=1200+I*200;
  OK=Wire(G,ClipNode,TEXT("Pose"),Choice,*FString::Printf(TEXT("BlendPose_%d"),I)) && OK;
  if(auto* Time=Choice->FindPin(*FString::Printf(TEXT("BlendTime_%d"),I)))Time->DefaultValue=I==11?TEXT("0.18"):TEXT("0.32");
 }
 Bind(TEXT("OceanPose"),Choice,TEXT("ActiveChildIndex"));
 auto* Blend=Add<UAnimGraphNode_TwoWayBlend>(G);Blend->NodeComment=TEXT("OceanExperienceLocomotionR2");
 Blend->NodePosX=1200;Blend->NodePosY=950;auto Targets=Pose->LinkedTo;Pose->BreakAllPinLinks();
 OK=Wire(G,WaterBlend,TEXT("Pose"),Blend,TEXT("A")) && OK;
 OK=Wire(G,Choice,TEXT("Pose"),Blend,TEXT("B")) && OK;Bind(TEXT("EquipmentWaterAlpha"),Blend,TEXT("Alpha"));
 for(auto* Target:Targets)OK=G->GetSchema()->TryCreateConnection(Blend->FindPin(TEXT("Pose")),Target) && OK;
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
 return OK && BP->Status!=BS_Error;
}
bool UVisualArtLibrary::BuildSwimmingGraph(UAnimBlueprint* BP,bool bProbeOnly)
{
    if(!BP || !BP->ParentClass || !BP->ParentClass->IsChildOf(USwimmingAnimInstance::StaticClass())) return false;
    auto* Graph=FindGraph(BP); if(!Graph) return false;
    UAnimGraphNode_Root* Root=nullptr;
    for(UEdGraphNode* Node:Graph->Nodes) if(auto* R=Cast<UAnimGraphNode_Root>(Node)) Root=R;
    // Refuse to discard manual graph edits. The import script handles idempotency.
    if(!Root || Graph->Nodes.Num()!=1) return false;
    BP->Modify(); Graph->Modify();
    FGraphNodeCreator<UAnimGraphNode_LinkedAnimGraph> LandCreator(*Graph);
    auto* Land=LandCreator.CreateNode();
    Land->Node.InstanceClass=LoadClass<UAnimInstance>(nullptr,TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C"));
    if(!Land->Node.InstanceClass) return false;
    LandCreator.Finalize(); Land->NodePosX=-1000; Land->NodePosY=-500;
    auto Player=[&](const TCHAR* Name,int32 Y)->UAnimGraphNode_SequencePlayer*
    {
        const FString Path=FString::Printf(TEXT("/Game/NeonCity/Animations/Swimming/AN_NC_Swim_%s.AN_NC_Swim_%s"),Name,Name);
        auto* Sequence=LoadObject<UAnimSequence>(nullptr,*Path); if(!Sequence) return nullptr;
        FGraphNodeCreator<UAnimGraphNode_SequencePlayer> Creator(*Graph);
        auto* Node=Creator.CreateNode(); Node->Node.SetSequence(Sequence); Node->Node.SetLoopAnimation(true);
        Creator.Finalize(); Node->NodePosX=-1400; Node->NodePosY=Y;
        for(auto& Pin:Node->ShowPinForProperties) if(Pin.PropertyName==TEXT("PlayRate")) Pin.bShowPin=true;
        Node->ReconstructNode();
        FGraphNodeCreator<UK2Node_VariableGet> RateCreator(*Graph);
        auto* Rate=RateCreator.CreateNode(); Rate->VariableReference.SetSelfMember(TEXT("SwimPlayRate"));
        RateCreator.Finalize(); Rate->NodePosX=-1650; Rate->NodePosY=Y+120;
        if(!Wire(Graph,Rate,TEXT("SwimPlayRate"),Node,TEXT("PlayRate"))) return nullptr;
        return Node;
    };
    bool bOK=true;
    auto Blend=[&](UEdGraphNode* A,UEdGraphNode* B,const TCHAR* Property,int32 X,int32 Y)->UEdGraphNode*
    {
        if(!A || !B) { bOK=false; return nullptr; }
        auto* Node=Add<UAnimGraphNode_TwoWayBlend>(Graph); Node->NodePosX=X; Node->NodePosY=Y;
        FGraphNodeCreator<UK2Node_VariableGet> Creator(*Graph);
        auto* Variable=Creator.CreateNode(); Variable->VariableReference.SetSelfMember(Property);
        Creator.Finalize(); Variable->NodePosX=X-150; Variable->NodePosY=Y+140;
        bOK=Wire(Graph,A,TEXT("Pose"),Node,TEXT("A")) && bOK;
        bOK=Wire(Graph,B,TEXT("Pose"),Node,TEXT("B")) && bOK;
        bOK=Wire(Graph,Variable,Property,Node,TEXT("Alpha")) && bOK;
        return Node;
    };
    UEdGraphNode* Swim=Player(TEXT("Tread"),0);
    if(!bProbeOnly)
    {
        Swim=Blend(Swim,Player(TEXT("Forward"),240),TEXT("MovingAlpha"),-1000,0);
        Swim=Blend(Swim,Player(TEXT("Underwater"),480),TEXT("UnderwaterAlpha"),-700,100);
        Swim=Blend(Swim,Player(TEXT("Ascend"),720),TEXT("AscendAlpha"),-400,200);
        Swim=Blend(Swim,Player(TEXT("Descend"),960),TEXT("DescendAlpha"),-100,300);
    }
    auto* Final=Blend(Land,Swim,TEXT("WaterAlpha"),300,0);
    bOK=Wire(Graph,Final,TEXT("Pose"),Root,TEXT("Result")) && bOK;
    Root->NodePosX=650;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FKismetEditorUtilities::CompileBlueprint(BP);
    return bOK && BP->Status!=BS_Error;
}

bool UVisualArtLibrary::BuildCharacterPresentationGraph(UAnimBlueprint* BP,bool bNPC)
{
 if(!BP || BP->ParentClass!=UCharacterPresentationAnimInstance::StaticClass()) return false;
 auto* Graph=FindGraph(BP); if(!Graph || Graph->Nodes.Num()!=1) return false;
 if(!bNPC && !BuildSwimmingGraph(BP,false)) return false;
 UAnimGraphNode_Root* Root=nullptr;
 for(UEdGraphNode* Node:Graph->Nodes) if(auto* R=Cast<UAnimGraphNode_Root>(Node)) Root=R;
 if(!Root) return false;
 bool bOK=true; int32 Row=0;
 auto Variable=[&](const TCHAR* Property,UEdGraphNode* To,FName Pin)
 {
  FGraphNodeCreator<UK2Node_VariableGet> Creator(*Graph);
  auto* Node=Creator.CreateNode(); Node->VariableReference.SetSelfMember(Property); Creator.Finalize();
  Node->NodePosX=600; Node->NodePosY=Row*160+100;
  bOK=Wire(Graph,Node,Property,To,Pin) && bOK;
 };
 auto Sequence=[&](const FString& Path,bool bLoop,const TCHAR* Time=nullptr)->UEdGraphNode*
 {
  auto* Asset=LoadObject<UAnimSequence>(nullptr,*Path); if(!Asset) { bOK=false;return nullptr; }
  if(Time)
  {
   FGraphNodeCreator<UAnimGraphNode_SequenceEvaluator> Creator(*Graph);
   auto* Node=Creator.CreateNode(); Node->Node.SetSequence(Asset);Node->Node.SetShouldLoop(false);Creator.Finalize();
   Node->NodePosX=1000;Node->NodePosY=Row++*160;Variable(Time,Node,TEXT("ExplicitTime"));return Node;
  }
  FGraphNodeCreator<UAnimGraphNode_SequencePlayer> Creator(*Graph);
  auto* Node=Creator.CreateNode();Node->Node.SetSequence(Asset);Node->Node.SetLoopAnimation(bLoop);Creator.Finalize();
  Node->NodePosX=1000;Node->NodePosY=Row++*160;
  if(Path.Contains(TEXT("Walk_Fwd")))
  {
   for(auto& Pin:Node->ShowPinForProperties) if(Pin.PropertyName==TEXT("PlayRate")) Pin.bShowPin=true;
   Node->ReconstructNode();Variable(TEXT("WalkRate"),Node,TEXT("PlayRate"));
  }
  return Node;
 };
 auto Clip=[&](const TCHAR* Name,const TCHAR* Time=nullptr)
 {return Sequence(FString::Printf(TEXT("/Game/NeonCity/Animations/VerticalSlice/AN_NC205C_%s.AN_NC205C_%s"),Name,Name),true,Time);};
 auto Blend=[&](UEdGraphNode* A,UEdGraphNode* B,const TCHAR* Alpha)->UEdGraphNode*
 {
  auto* Node=Add<UAnimGraphNode_TwoWayBlend>(Graph);Node->NodePosX=1500;Node->NodePosY=Row++*160;
  bOK=Wire(Graph,A,TEXT("Pose"),Node,TEXT("A")) && bOK;
  bOK=Wire(Graph,B,TEXT("Pose"),Node,TEXT("B")) && bOK;Variable(Alpha,Node,TEXT("Alpha"));return Node;
 };
 auto Layer=[&](UEdGraphNode* A,UEdGraphNode* B,const TCHAR* Alpha)->UEdGraphNode*
 {
  FGraphNodeCreator<UAnimGraphNode_LayeredBoneBlend> Creator(*Graph);auto* Node=Creator.CreateNode();
  Node->Node.BlendPoses.Reset();Node->Node.BlendWeights.Reset();Node->Node.LayerSetup.Reset();Node->Node.AddPose();
  FBranchFilter Filter;Filter.BoneName=TEXT("spine_01");Filter.BlendDepth=3;
  Node->Node.LayerSetup[0].BranchFilters.Add(Filter);Node->Node.bMeshSpaceRotationBlend=true;
  Creator.Finalize();Node->NodePosX=2000;Node->NodePosY=Row++*160;
  bOK=Wire(Graph,A,TEXT("Pose"),Node,TEXT("BasePose")) && bOK;
  bOK=Wire(Graph,B,TEXT("Pose"),Node,TEXT("BlendPoses_0")) && bOK;
  Variable(Alpha,Node,TEXT("BlendWeights_0"));return Node;
 };
 UEdGraphNode* Base=nullptr;
 if(bNPC)
 {
  Base=Blend(Sequence(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"),true),
   Sequence(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd.MF_Unarmed_Walk_Fwd"),true),TEXT("WalkAlpha"));
 }
 else
 {
  auto* Pin=Root->FindPin(TEXT("Result"));if(!Pin || Pin->LinkedTo.Num()!=1)return false;
  Base=Pin->LinkedTo[0]->GetOwningNode();Pin->BreakAllPinLinks();
 }
 auto* Aim=Blend(Clip(TEXT("Pistol_Aim_Neutral")),Clip(TEXT("Pistol_Aim_Up")),TEXT("AimUpAlpha"));
 Aim=Blend(Aim,Clip(TEXT("Pistol_Aim_Down")),TEXT("AimDownAlpha"));
 auto* Weapon=Blend(Clip(TEXT("Pistol_Idle_Loop")),Aim,TEXT("AimAlpha"));
 Weapon=Blend(Weapon,Clip(TEXT("Pistol_Shoot"),TEXT("ShotTime")),TEXT("ShotAlpha"));
 Weapon=Blend(Weapon,Clip(TEXT("Pistol_Reload"),TEXT("ReloadTime")),TEXT("ReloadAlpha"));
 Base=Layer(Base,Weapon,TEXT("ArmedAlpha"));
 // Injury holds a subtle chest-protecting frame. Actual hit and collapse are
 // driven by unified health feedback, not by a police/player special case.
 auto* Injury=Cast<UAnimGraphNode_SequenceEvaluator>(Clip(TEXT("Hit_Chest"),TEXT("HitTime")));
 if(Injury)
 {
  auto* Pin=Injury->FindPin(TEXT("ExplicitTime"));if(Pin) {Pin->BreakAllPinLinks();Pin->DefaultValue=TEXT("0.16");}
 }
 Base=Layer(Base,Injury,TEXT("InjuryAlpha"));
 Base=Layer(Base,Clip(TEXT("Hit_Chest"),TEXT("HitTime")),TEXT("HitAlpha"));
 Base=Blend(Base,Clip(TEXT("Death01"),TEXT("CollapseTime")),TEXT("IncapacitatedAlpha"));
 bOK=Wire(Graph,Base,TEXT("Pose"),Root,TEXT("Result")) && bOK;
 Root->NodePosX=2500;FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
 return bOK && BP->Status!=BS_Error;
}

bool UVisualArtLibrary::AddWeaponSupportContact(UAnimBlueprint* BP)
{
 if(!BP || BP->ParentClass!=UCharacterPresentationAnimInstance::StaticClass())return false;
 auto* Graph=FindGraph(BP);if(!Graph)return false;
 UAnimGraphNode_Root* Root=nullptr;
 for(UEdGraphNode* Node:Graph->Nodes)
 {
  if(Node->NodeComment==TEXT("NeonCity.SupportHand"))return true;
  if(auto* R=Cast<UAnimGraphNode_Root>(Node))Root=R;
 }
 auto* Result=Root?Root->FindPin(TEXT("Result")):nullptr;
 if(!Result || Result->LinkedTo.Num()!=1)return false;
 auto* Previous=Result->LinkedTo[0];Result->BreakAllPinLinks();
 auto* ToComponent=Add<UAnimGraphNode_LocalToComponentSpace>(Graph);
 bool OK=Wire(Graph,Previous->GetOwningNode(),Previous->PinName,ToComponent,TEXT("LocalPose"));
 FGraphNodeCreator<UAnimGraphNode_TwoBoneIK> Creator(*Graph);auto* IK=Creator.CreateNode();
 IK->Node.IKBone.BoneName=TEXT("hand_l");IK->Node.EffectorTarget.BoneReference.BoneName=TEXT("hand_r");
 IK->Node.EffectorLocationSpace=BCS_BoneSpace;IK->Node.JointTargetLocationSpace=BCS_ComponentSpace;
 IK->Node.bAllowStretching=false;IK->Node.bTakeRotationFromEffectorSpace=false;IK->Node.bMaintainEffectorRelRot=false;
 Creator.Finalize();IK->NodeComment=TEXT("NeonCity.SupportHand");
 for(auto& Pin:IK->ShowPinForProperties)if(Pin.PropertyName==TEXT("Alpha"))Pin.bShowPin=true;
 IK->ReconstructNode();
 auto Bind=[&](const TCHAR* Property,const TCHAR* Pin)
 {
  FGraphNodeCreator<UK2Node_VariableGet> V(*Graph);auto* Node=V.CreateNode();
  Node->VariableReference.SetSelfMember(Property);V.Finalize();OK=Wire(Graph,Node,Property,IK,Pin) && OK;
 };
 Bind(TEXT("SupportHandRightSpace"),TEXT("EffectorLocation"));Bind(TEXT("SupportElbow"),TEXT("JointTargetLocation"));Bind(TEXT("SupportHandAlpha"),TEXT("Alpha"));
 auto* ToLocal=Add<UAnimGraphNode_ComponentToLocalSpace>(Graph);
 OK=Wire(Graph,ToComponent,TEXT("ComponentPose"),IK,TEXT("ComponentPose")) && OK;
 OK=Wire(Graph,IK,TEXT("Pose"),ToLocal,TEXT("ComponentPose")) && OK;
 OK=Wire(Graph,ToLocal,TEXT("Pose"),Root,TEXT("Result")) && OK;
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
 return OK && BP->Status!=BS_Error;
}

bool UVisualArtLibrary::BuildTransportOccupantGraph(UAnimBlueprint* BP)
{
 if(!BP || BP->ParentClass!=UTransportOccupantAnimInstance::StaticClass())return false;
 auto* Graph=FindGraph(BP);if(!Graph || Graph->Nodes.Num()!=1)return false;
 auto* Root=Cast<UAnimGraphNode_Root>(Graph->Nodes[0]);if(!Root)return false;
 auto* Clip=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/NeonCity/Animations/VerticalSlice/AN_NC205C_Driving_Loop.AN_NC205C_Driving_Loop"));
 if(!Clip)return false;
 FGraphNodeCreator<UAnimGraphNode_SequencePlayer> Creator(*Graph);auto* Player=Creator.CreateNode();
 Player->Node.SetSequence(Clip);Player->Node.SetLoopAnimation(true);Creator.Finalize();
 auto* ToComponent=Add<UAnimGraphNode_LocalToComponentSpace>(Graph);
 bool OK=Wire(Graph,Player,TEXT("Pose"),ToComponent,TEXT("LocalPose"));
 UEdGraphNode* Last=ToComponent;
 struct FLimb { const TCHAR* Bone;const TCHAR* Effector;const TCHAR* Pole; };
 const FLimb Limbs[]={{TEXT("hand_l"),TEXT("LeftHand"),TEXT("LeftElbow")},{TEXT("hand_r"),TEXT("RightHand"),TEXT("RightElbow")},
  {TEXT("foot_l"),TEXT("LeftFoot"),TEXT("LeftKnee")},{TEXT("foot_r"),TEXT("RightFoot"),TEXT("RightKnee")}};
 int32 Column=1;
 for(const auto& Limb:Limbs)
 {
  FGraphNodeCreator<UAnimGraphNode_TwoBoneIK> IKCreator(*Graph);auto* IK=IKCreator.CreateNode();
  IK->Node.IKBone.BoneName=Limb.Bone;IK->Node.EffectorLocationSpace=BCS_ComponentSpace;IK->Node.JointTargetLocationSpace=BCS_ComponentSpace;
  IK->Node.bAllowStretching=false;IK->Node.bMaintainEffectorRelRot=false;IKCreator.Finalize();
  IK->NodePosX=Column++*350;
  OK=Wire(Graph,Last,Last==ToComponent?TEXT("ComponentPose"):TEXT("Pose"),IK,TEXT("ComponentPose")) && OK;
  auto Variable=[&](const TCHAR* Property,const TCHAR* Pin,int32 Row)
  {
   FGraphNodeCreator<UK2Node_VariableGet> V(*Graph);auto* Node=V.CreateNode();
   Node->VariableReference.SetSelfMember(Property);V.Finalize();Node->NodePosX=IK->NodePosX-250;Node->NodePosY=Row;
   OK=Wire(Graph,Node,Property,IK,Pin) && OK;
  };
  Variable(Limb.Effector,TEXT("EffectorLocation"),150);Variable(Limb.Pole,TEXT("JointTargetLocation"),260);Last=IK;
 }
 auto* ToLocal=Add<UAnimGraphNode_ComponentToLocalSpace>(Graph);
 OK=Wire(Graph,Last,TEXT("Pose"),ToLocal,TEXT("ComponentPose")) && OK;
 OK=Wire(Graph,ToLocal,TEXT("Pose"),Root,TEXT("Result")) && OK;
 ToLocal->NodePosX=Column++*350;Root->NodePosX=Column*350;
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
 return OK && BP->Status!=BS_Error;
}

bool UVisualArtLibrary::AddFishingHandContact(UAnimBlueprint* BP)
{
 if(!BP || BP->ParentClass!=UCharacterPresentationAnimInstance::StaticClass())return false;
 auto* Graph=FindGraph(BP);if(!Graph)return false;UAnimGraphNode_Root* Root=nullptr;
 for(UEdGraphNode* Node:Graph->Nodes)
 {
  if(Node->NodeComment==TEXT("NeonCity.FishingHands"))return true;
  if(auto* Candidate=Cast<UAnimGraphNode_Root>(Node))Root=Candidate;
 }
 auto* Result=Root?Root->FindPin(TEXT("Result")):nullptr;if(!Result || Result->LinkedTo.Num()!=1)return false;
 auto* Previous=Result->LinkedTo[0];Result->BreakAllPinLinks();
 auto* ToComponent=Add<UAnimGraphNode_LocalToComponentSpace>(Graph);
 bool OK=Wire(Graph,Previous->GetOwningNode(),Previous->PinName,ToComponent,TEXT("LocalPose"));
 UEdGraphNode* Last=ToComponent;
 struct FLimb { const TCHAR* Bone;const TCHAR* Target;const TCHAR* Pole; };
 const FLimb Limbs[]={{TEXT("hand_r"),TEXT("FishingRightHand"),TEXT("FishingRightElbow")},{TEXT("hand_l"),TEXT("FishingLeftHand"),TEXT("FishingLeftElbow")}};
 for(const auto& Limb:Limbs)
 {
  FGraphNodeCreator<UAnimGraphNode_TwoBoneIK> Creator(*Graph);auto* IK=Creator.CreateNode();
  IK->Node.IKBone.BoneName=Limb.Bone;IK->Node.EffectorLocationSpace=BCS_ComponentSpace;IK->Node.JointTargetLocationSpace=BCS_ComponentSpace;
  IK->Node.bAllowStretching=false;IK->Node.bTakeRotationFromEffectorSpace=false;IK->Node.bMaintainEffectorRelRot=true;
  Creator.Finalize();IK->NodeComment=TEXT("NeonCity.FishingHands");
  for(auto& Pin:IK->ShowPinForProperties)if(Pin.PropertyName==TEXT("Alpha"))Pin.bShowPin=true;IK->ReconstructNode();
  const auto Bind=[&](const TCHAR* Property,const TCHAR* Pin)
  {
   FGraphNodeCreator<UK2Node_VariableGet> V(*Graph);auto* Node=V.CreateNode();Node->VariableReference.SetSelfMember(Property);V.Finalize();
   OK=Wire(Graph,Node,Property,IK,Pin) && OK;
  };
  Bind(Limb.Target,TEXT("EffectorLocation"));Bind(Limb.Pole,TEXT("JointTargetLocation"));Bind(TEXT("FishingAlpha"),TEXT("Alpha"));
  OK=Wire(Graph,Last,Last==ToComponent?TEXT("ComponentPose"):TEXT("Pose"),IK,TEXT("ComponentPose")) && OK;Last=IK;
 }
 auto* ToLocal=Add<UAnimGraphNode_ComponentToLocalSpace>(Graph);
 OK=Wire(Graph,Last,TEXT("Pose"),ToLocal,TEXT("ComponentPose")) && OK;
 OK=Wire(Graph,ToLocal,TEXT("Pose"),Root,TEXT("Result")) && OK;
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
 return OK && BP->Status!=BS_Error;
}

bool UVisualArtLibrary::AddFishingWristContact(UAnimBlueprint* BP)
{
 auto* G=BP?FindGraph(BP):nullptr;if(!G)return false;
 for(UEdGraphNode* N:G->Nodes)if(N->NodeComment==TEXT("NeonCity.FishingWristContact"))return true;
 UAnimGraphNode_TwoBoneIK* Last=nullptr;
 for(UEdGraphNode* N:G->Nodes)if(auto* IK=Cast<UAnimGraphNode_TwoBoneIK>(N))
  if(IK->NodeComment==TEXT("NeonCity.FishingHands") && IK->Node.IKBone.BoneName==TEXT("hand_l"))Last=IK;
 auto* Out=Last?Last->FindPin(TEXT("Pose")):nullptr;if(!Out || Out->LinkedTo.Num()!=1)return false;
 auto* Destination=Out->LinkedTo[0];BP->Modify();G->Modify();Out->BreakAllPinLinks();
 UEdGraphNode* Previous=Last;bool OK=true;
 for(const bool Right:{true,false})
 {
  FGraphNodeCreator<UAnimGraphNode_ModifyBone> C(*G);auto* N=C.CreateNode();
  N->Node.BoneToModify.BoneName=Right?TEXT("hand_r"):TEXT("hand_l");
  N->Node.RotationMode=BMM_Replace;N->Node.RotationSpace=BCS_ComponentSpace;
  C.Finalize();N->NodeComment=TEXT("NeonCity.FishingWristContact");
  for(auto& P:N->ShowPinForProperties)if(P.PropertyName==TEXT("Rotation") || P.PropertyName==TEXT("Alpha"))P.bShowPin=true;
  N->ReconstructNode();
  for(const TCHAR* Property:{Right?TEXT("FishingRightWrist"):TEXT("FishingLeftWrist"),TEXT("FishingAlpha")})
  {
   FGraphNodeCreator<UK2Node_VariableGet> V(*G);auto* Getter=V.CreateNode();Getter->VariableReference.SetSelfMember(Property);V.Finalize();
   OK=Wire(G,Getter,Property,N,FString(Property)==TEXT("FishingAlpha")?TEXT("Alpha"):TEXT("Rotation")) && OK;
  }
  OK=Wire(G,Previous,TEXT("Pose"),N,TEXT("ComponentPose")) && OK;Previous=N;
 }
 OK=Wire(G,Previous,TEXT("Pose"),Destination->GetOwningNode(),Destination->PinName) && OK;
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
 return OK && BP->Status!=BS_Error;
}

bool UVisualArtLibrary::AddCookingBodyContact(UAnimBlueprint* BP)
{
 auto* G=BP?FindGraph(BP):nullptr;if(!G)return false;
 for(UEdGraphNode* N:G->Nodes)if(N->NodeComment==TEXT("NeonCity.CookingBodyContact"))return true;
 UAnimGraphNode_TwoBoneIK* First=nullptr;
 for(UEdGraphNode* N:G->Nodes)if(auto* IK=Cast<UAnimGraphNode_TwoBoneIK>(N))
  if(IK->NodeComment==TEXT("NeonCity.FishingHands") && IK->Node.IKBone.BoneName==TEXT("hand_r"))First=IK;
 auto* Input=First?First->FindPin(TEXT("ComponentPose")):nullptr;if(!Input || Input->LinkedTo.Num()!=1)return false;
 auto* Previous=Input->LinkedTo[0];BP->Modify();G->Modify();Input->BreakAllPinLinks();
 FGraphNodeCreator<UAnimGraphNode_ModifyBone> Creator(*G);auto* Lean=Creator.CreateNode();
 Lean->Node.BoneToModify.BoneName=TEXT("spine_01");Lean->Node.RotationMode=BMM_Additive;Lean->Node.RotationSpace=BCS_ComponentSpace;
 Creator.Finalize();Lean->NodeComment=TEXT("NeonCity.CookingBodyContact");
 for(auto& P:Lean->ShowPinForProperties)if(P.PropertyName==TEXT("Rotation") || P.PropertyName==TEXT("Alpha"))P.bShowPin=true;
 Lean->ReconstructNode();bool OK=Wire(G,Previous->GetOwningNode(),Previous->PinName,Lean,TEXT("ComponentPose"));
 for(const TCHAR* Property:{TEXT("CookingSpineRotation"),TEXT("CookingBodyAlpha")})
 {
  FGraphNodeCreator<UK2Node_VariableGet> C(*G);auto* V=C.CreateNode();V->VariableReference.SetSelfMember(Property);C.Finalize();
  OK=Wire(G,V,Property,Lean,FString(Property)==TEXT("CookingBodyAlpha")?TEXT("Alpha"):TEXT("Rotation")) && OK;
 }
 OK=Wire(G,Lean,TEXT("Pose"),First,TEXT("ComponentPose")) && OK;
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
 return OK && BP->Status!=BS_Error;
}

bool UVisualArtLibrary::AddFishingMotionR2(UAnimBlueprint* BP)
{
 auto* G=BP?FindGraph(BP):nullptr;if(!G)return false;
 for(UEdGraphNode* N:G->Nodes)if(N->NodeComment==TEXT("NeonCity.FishingMotionR2"))return true;
 UAnimGraphNode_TwoBoneIK* Right=nullptr;
 for(UEdGraphNode* N:G->Nodes)if(auto* IK=Cast<UAnimGraphNode_TwoBoneIK>(N))
  if(IK->NodeComment==TEXT("NeonCity.FishingHands") && IK->Node.IKBone.BoneName==TEXT("hand_r"))Right=IK;
 auto* Input=Right?Right->FindPin(TEXT("ComponentPose")):nullptr;
 if(!Input || Input->LinkedTo.Num()!=1)return false;
 auto* Conversion=Cast<UAnimGraphNode_LocalToComponentSpace>(Input->LinkedTo[0]->GetOwningNode());
 auto* Local=Conversion?Conversion->FindPin(TEXT("LocalPose")):nullptr;
 if(!Local || Local->LinkedTo.Num()!=1)return false;
 TArray<UAnimSequence*> Clips;
 for(const TCHAR* Name:{TEXT("Ready"),TEXT("Windup"),TEXT("Cast"),TEXT("Fight")})
 {
  auto* Clip=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/NeonCity/Art/OceanExperience/Fishing/R2/Motion/A_FishingR2_%s"),Name));
  if(!Clip || Clip->GetSkeleton()!=BP->TargetSkeleton)return false;Clips.Add(Clip);
 }
 BP->Modify();G->Modify();bool OK=true;
 auto Bind=[&](const TCHAR* Name,UEdGraphNode* To,const TCHAR* Pin)
 {
  FGraphNodeCreator<UK2Node_VariableGet> C(*G);auto* V=C.CreateNode();V->VariableReference.SetSelfMember(Name);C.Finalize();
  OK=Wire(G,V,Name,To,Pin) && OK;
 };
 auto* Choice=Add<UAnimGraphNode_BlendListByInt>(G);
 while(Choice->Node.GetBlendTimes().Num()<4)Choice->AddPinToBlendList();
 for(int32 I=0;I<4;++I)
 {
  UEdGraphNode* Pose=nullptr;
  if(I==1 || I==2)
  {
   FGraphNodeCreator<UAnimGraphNode_SequenceEvaluator> C(*G);auto* N=C.CreateNode();N->Node.SetSequence(Clips[I]);C.Finalize();
   for(auto& P:N->ShowPinForProperties)if(P.PropertyName==TEXT("ExplicitTime"))P.bShowPin=true;
   N->ReconstructNode();Bind(TEXT("FishingPoseTime"),N,TEXT("ExplicitTime"));Pose=N;
  }
  else
  {
   FGraphNodeCreator<UAnimGraphNode_SequencePlayer> C(*G);auto* N=C.CreateNode();N->Node.SetSequence(Clips[I]);N->Node.SetLoopAnimation(true);C.Finalize();Pose=N;
  }
  OK=Wire(G,Pose,TEXT("Pose"),Choice,*FString::Printf(TEXT("BlendPose_%d"),I)) && OK;
  if(auto* P=Choice->FindPin(*FString::Printf(TEXT("BlendTime_%d"),I)))P->DefaultValue=I==2?TEXT("0.035"):TEXT("0.18");
 }
 Bind(TEXT("FishingPose"),Choice,TEXT("ActiveChildIndex"));
 auto* Previous=Local->LinkedTo[0];Local->BreakAllPinLinks();
 auto* Blend=Add<UAnimGraphNode_TwoWayBlend>(G);Blend->NodeComment=TEXT("NeonCity.FishingMotionR2");
 OK=Wire(G,Previous->GetOwningNode(),Previous->PinName,Blend,TEXT("A")) && OK;
 OK=Wire(G,Choice,TEXT("Pose"),Blend,TEXT("B")) && OK;Bind(TEXT("FishingAlpha"),Blend,TEXT("Alpha"));
 OK=Wire(G,Blend,TEXT("Pose"),Conversion,TEXT("LocalPose")) && OK;
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
 return OK && BP->Status!=BS_Error;
}

bool UVisualArtLibrary::SeparateFishingBodyWeight(UAnimBlueprint* BP)
{
 if(!BP || BP->ParentClass!=UCharacterPresentationAnimInstance::StaticClass())return false;
 auto* G=FindGraph(BP);if(!G)return false;
 for(UEdGraphNode* Node:G->Nodes)if(Node->NodeComment==TEXT("NeonCity.FishingMotionR2"))
 {
  auto* Blend=Cast<UAnimGraphNode_TwoWayBlend>(Node);auto* Alpha=Blend?Blend->FindPin(TEXT("Alpha")):nullptr;if(!Alpha)return false;
  if(Alpha->LinkedTo.Num()==1)if(auto* V=Cast<UK2Node_VariableGet>(Alpha->LinkedTo[0]->GetOwningNode());V && V->VariableReference.GetMemberName()==TEXT("FishingBodyAlpha"))return true;
  BP->Modify();G->Modify();Alpha->BreakAllPinLinks();
  auto* V=Add<UK2Node_VariableGet>(G);V->VariableReference.SetSelfMember(TEXT("FishingBodyAlpha"));V->ReconstructNode();
  if(!Wire(G,V,TEXT("FishingBodyAlpha"),Blend,TEXT("Alpha")))return false;
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);return BP->Status!=BS_Error;
 }
 return false;
}
bool UVisualArtLibrary::AddFlashlightCarryPose(UAnimBlueprint* BP,UAnimSequence* Clip)
{
 if(!BP || !Clip || BP->ParentClass!=UCharacterPresentationAnimInstance::StaticClass() || Clip->GetSkeleton()!=BP->TargetSkeleton)return false;
 auto* Graph=FindGraph(BP);if(!Graph)return false;
 UAnimGraphNode_Root* Root=nullptr;
 for(UEdGraphNode* Node:Graph->Nodes)
 {
  if(Node->NodeComment==TEXT("NeonCity.FlashlightCarry"))
  {
   auto* Pin=Node->FindPin(TEXT("BlendPoses_0"));
   auto* Existing=Pin && Pin->LinkedTo.Num()==1?Cast<UAnimGraphNode_SequencePlayer>(Pin->LinkedTo[0]->GetOwningNode()):nullptr;
   if(!Existing)return false;
   Existing->Node.SetSequence(Clip);
   FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
   return BP->Status!=BS_Error;
  }
  if(auto* Candidate=Cast<UAnimGraphNode_Root>(Node))Root=Candidate;
 }
 auto* Result=Root?Root->FindPin(TEXT("Result")):nullptr;
 if(!Result || Result->LinkedTo.Num()!=1 || !Clip)return false;
 auto* Previous=Result->LinkedTo[0];Result->BreakAllPinLinks();
 FGraphNodeCreator<UAnimGraphNode_SequencePlayer> Sequence(*Graph);auto* Pose=Sequence.CreateNode();
 Pose->Node.SetSequence(Clip);Pose->Node.SetLoopAnimation(true);Sequence.Finalize();
 FGraphNodeCreator<UAnimGraphNode_LayeredBoneBlend> Layer(*Graph);auto* Blend=Layer.CreateNode();
 Blend->Node.BlendPoses.Reset();Blend->Node.BlendWeights.Reset();Blend->Node.LayerSetup.Reset();Blend->Node.AddPose();
 // One hand holds the utility; left arm and template locomotion remain free.
 FBranchFilter Filter;Filter.BoneName=TEXT("clavicle_r");Filter.BlendDepth=1;
 Blend->Node.LayerSetup[0].BranchFilters.Add(Filter);Blend->Node.bMeshSpaceRotationBlend=true;
 Layer.Finalize();Blend->NodeComment=TEXT("NeonCity.FlashlightCarry");
 FGraphNodeCreator<UK2Node_VariableGet> Variable(*Graph);auto* Weight=Variable.CreateNode();
 Weight->VariableReference.SetSelfMember(TEXT("FlashlightAlpha"));Variable.Finalize();
 bool OK=Wire(Graph,Previous->GetOwningNode(),Previous->PinName,Blend,TEXT("BasePose"));
 OK=Wire(Graph,Pose,TEXT("Pose"),Blend,TEXT("BlendPoses_0")) && OK;
 OK=Wire(Graph,Weight,TEXT("FlashlightAlpha"),Blend,TEXT("BlendWeights_0")) && OK;
 OK=Wire(Graph,Blend,TEXT("Pose"),Root,TEXT("Result")) && OK;
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
 return OK && BP->Status!=BS_Error;
}

bool UVisualArtLibrary::AddDivingGearTransition(UAnimBlueprint* BP,UAnimSequence* Clip)
{
 if(!BP || !Clip || BP->ParentClass!=UCharacterPresentationAnimInstance::StaticClass() || Clip->GetSkeleton()!=BP->TargetSkeleton)return false;
 auto* G=FindGraph(BP);if(!G)return false;UAnimGraphNode_Root* Root=nullptr;
 for(UEdGraphNode* N:G->Nodes)
 {
  if(N->NodeComment==TEXT("NeonCity.DivingGearTransition"))return true;
  if(auto* R=Cast<UAnimGraphNode_Root>(N))Root=R;
 }
 auto* Result=Root?Root->FindPin(TEXT("Result")):nullptr;if(!Result || Result->LinkedTo.Num()!=1)return false;
 BP->Modify();G->Modify();auto* Previous=Result->LinkedTo[0];Result->BreakAllPinLinks();
 FGraphNodeCreator<UAnimGraphNode_SequenceEvaluator> C(*G);auto* Pose=C.CreateNode();Pose->Node.SetSequence(Clip);C.Finalize();
 for(auto& P:Pose->ShowPinForProperties)if(P.PropertyName==TEXT("ExplicitTime"))P.bShowPin=true;
 Pose->ReconstructNode();auto* Blend=Add<UAnimGraphNode_TwoWayBlend>(G);Blend->NodeComment=TEXT("NeonCity.DivingGearTransition");
 bool OK=Wire(G,Previous->GetOwningNode(),Previous->PinName,Blend,TEXT("A"));
 OK=Wire(G,Pose,TEXT("Pose"),Blend,TEXT("B")) && OK;
 auto Bind=[&](const TCHAR* Name,UEdGraphNode* To,const TCHAR* Pin)
 {
  FGraphNodeCreator<UK2Node_VariableGet> V(*G);auto* N=V.CreateNode();N->VariableReference.SetSelfMember(Name);V.Finalize();
  OK=Wire(G,N,Name,To,Pin) && OK;
 };
 Bind(TEXT("GearChangeTime"),Pose,TEXT("ExplicitTime"));Bind(TEXT("GearChangeAlpha"),Blend,TEXT("Alpha"));
 OK=Wire(G,Blend,TEXT("Pose"),Root,TEXT("Result")) && OK;
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
 return OK && BP->Status!=BS_Error;
}
