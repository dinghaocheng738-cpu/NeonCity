#include "CoastalAmbientArtLibrary.h"
#include "Presentation/CoastalAmbientAnimInstance.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_BlendListByInt.h"
#include "AnimGraphNode_TwoWayBlend.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "AnimGraphNode_TwoBoneIK.h"
#include "AnimGraphNode_ModifyBone.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"

bool UCoastalAmbientArtLibrary::AddCoastalAmbientLayers(UAnimBlueprint* BP,UAnimSequence* Idle,UAnimSequence* Look,UAnimSequence* Talk,UAnimSequence* Sitting)
{
 if(!BP || BP->GetPathName()!=TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC921/ABP_C921_CoastalNPC.ABP_C921_CoastalNPC")
  || BP->ParentClass!=UCoastalAmbientAnimInstance::StaticClass() || !Idle || !Look || !Talk || !Sitting)return false;
 for(auto* Clip:{Idle,Look,Talk,Sitting})if(Clip->GetSkeleton()!=BP->TargetSkeleton)return false;
 UEdGraph* G=nullptr;for(UEdGraph* Graph:BP->FunctionGraphs)if(Graph && Graph->GetFName()==TEXT("AnimGraph"))G=Graph;
 if(!G)return false;UAnimGraphNode_Root* Root=nullptr;
 for(UEdGraphNode* Node:G->Nodes){if(Node->NodeComment==TEXT("NeonCity.C921.AmbientLayers"))return false;if(auto* R=Cast<UAnimGraphNode_Root>(Node))Root=R;}
 auto* Result=Root?Root->FindPin(TEXT("Result")):nullptr;if(!Result || Result->LinkedTo.Num()!=1)return false;
 BP->Modify();G->Modify();bool OK=true;auto* Previous=Result->LinkedTo[0];Result->BreakAllPinLinks();
 auto Wire=[&](UEdGraphPin* A,UEdGraphPin* B){const bool Connected=A && B && G->GetSchema()->TryCreateConnection(A,B);OK=Connected && OK;};
 auto Bind=[&](const TCHAR* Property,UEdGraphNode* To,const TCHAR* Pin)
 {
  FGraphNodeCreator<UK2Node_VariableGet> C(*G);auto* V=C.CreateNode();V->VariableReference.SetSelfMember(Property);C.Finalize();
  Wire(V->FindPin(Property,EGPD_Output),To->FindPin(Pin,EGPD_Input));
 };
 auto Player=[&](UAnimSequence* Clip,const TCHAR* Offset)
 {
  FGraphNodeCreator<UAnimGraphNode_SequencePlayer> C(*G);auto* P=C.CreateNode();P->Node.SetSequence(Clip);P->Node.SetLoopAnimation(true);C.Finalize();
  if(Offset){for(auto& Pin:P->ShowPinForProperties)if(Pin.PropertyName==TEXT("StartPosition"))Pin.bShowPin=true;P->ReconstructNode();Bind(Offset,P,TEXT("StartPosition"));}
  P->NodeComment=TEXT("NeonCity.C921.LicensedActivity");return P;
 };
 FGraphNodeCreator<UAnimGraphNode_TwoWayBlend> SC(*G);auto* Seat=SC.CreateNode();SC.Finalize();
 Wire(Previous,Seat->FindPin(TEXT("A")));Wire(Player(Sitting,nullptr)->FindPin(TEXT("Pose")),Seat->FindPin(TEXT("B")));Bind(TEXT("AmbientSeatAlpha"),Seat,TEXT("Alpha"));
 FGraphNodeCreator<UAnimGraphNode_BlendListByInt> CC(*G);auto* Choice=CC.CreateNode();CC.Finalize();
 while(Choice->Node.GetBlendTimes().Num()<3)Choice->AddPinToBlendList();
 UAnimSequence* Clips[]={Idle,Look,Talk};const TCHAR* Offsets[]={TEXT("AmbientIdleOffset"),TEXT("AmbientLookOffset"),TEXT("AmbientTalkOffset")};
 for(int32 I=0;I<3;++I){Wire(Player(Clips[I],Offsets[I])->FindPin(TEXT("Pose")),Choice->FindPin(*FString::Printf(TEXT("BlendPose_%d"),I)));if(auto* T=Choice->FindPin(*FString::Printf(TEXT("BlendTime_%d"),I)))T->DefaultValue=TEXT("0.45");}
 Bind(TEXT("AmbientActivityIndex"),Choice,TEXT("ActiveChildIndex"));
 FGraphNodeCreator<UAnimGraphNode_LayeredBoneBlend> LC(*G);auto* Layer=LC.CreateNode();
 Layer->Node.BlendPoses.Reset();Layer->Node.BlendWeights.Reset();Layer->Node.LayerSetup.Reset();Layer->Node.AddPose();
 FBranchFilter Filter;Filter.BoneName=TEXT("spine_01");Filter.BlendDepth=3;Layer->Node.LayerSetup[0].BranchFilters.Add(Filter);Layer->Node.bMeshSpaceRotationBlend=true;LC.Finalize();
 Layer->NodeComment=TEXT("NeonCity.C921.AmbientLayers");
 Wire(Seat->FindPin(TEXT("Pose")),Layer->FindPin(TEXT("BasePose")));Wire(Choice->FindPin(TEXT("Pose")),Layer->FindPin(TEXT("BlendPoses_0")));Bind(TEXT("AmbientAlpha"),Layer,TEXT("BlendWeights_0"));
 FGraphNodeCreator<UAnimGraphNode_LocalToComponentSpace> TC(*G);auto* ToComponent=TC.CreateNode();TC.Finalize();Wire(Layer->FindPin(TEXT("Pose")),ToComponent->FindPin(TEXT("LocalPose")));
 FGraphNodeCreator<UAnimGraphNode_TwoBoneIK> IC(*G);auto* IK=IC.CreateNode();IK->Node.IKBone.BoneName=TEXT("hand_r");IK->Node.EffectorLocationSpace=BCS_ComponentSpace;IK->Node.JointTargetLocationSpace=BCS_ComponentSpace;IK->Node.bAllowStretching=false;IK->Node.bMaintainEffectorRelRot=true;IC.Finalize();
 for(auto& Pin:IK->ShowPinForProperties)if(Pin.PropertyName==TEXT("Alpha"))Pin.bShowPin=true;IK->ReconstructNode();
 Bind(TEXT("AmbientCupAlpha"),IK,TEXT("Alpha"));Bind(TEXT("AmbientCupHand"),IK,TEXT("EffectorLocation"));Bind(TEXT("AmbientCupElbow"),IK,TEXT("JointTargetLocation"));Wire(ToComponent->FindPin(TEXT("ComponentPose")),IK->FindPin(TEXT("ComponentPose")));
 FGraphNodeCreator<UAnimGraphNode_ModifyBone> WC(*G);auto* Wrist=WC.CreateNode();Wrist->Node.BoneToModify.BoneName=TEXT("hand_r");Wrist->Node.RotationMode=BMM_Replace;Wrist->Node.RotationSpace=BCS_ComponentSpace;WC.Finalize();
 for(auto& Pin:Wrist->ShowPinForProperties)if(Pin.PropertyName==TEXT("Rotation") || Pin.PropertyName==TEXT("Alpha"))Pin.bShowPin=true;Wrist->ReconstructNode();Bind(TEXT("AmbientCupWrist"),Wrist,TEXT("Rotation"));Bind(TEXT("AmbientCupAlpha"),Wrist,TEXT("Alpha"));Wire(IK->FindPin(TEXT("Pose")),Wrist->FindPin(TEXT("ComponentPose")));
 FGraphNodeCreator<UAnimGraphNode_ComponentToLocalSpace> TLC(*G);auto* ToLocal=TLC.CreateNode();TLC.Finalize();Wire(Wrist->FindPin(TEXT("Pose")),ToLocal->FindPin(TEXT("ComponentPose")));Wire(ToLocal->FindPin(TEXT("Pose")),Result);
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);return OK && BP->Status!=BS_Error;
}

namespace CoastalCafeFeet52
{
constexpr const TCHAR* PrivatePath=TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC923/ABP_C923_Cafe52Grounded.ABP_C923_Cafe52Grounded");
constexpr const TCHAR* OriginalPath=TEXT("/Game/NeonCity/Art/Phase205cUpgrade/MainlandC921/ABP_C921_CoastalNPC.ABP_C921_CoastalNPC");
constexpr const TCHAR* LeftComment=TEXT("NeonCity.Cafe52.GroundFeet.Left");
constexpr const TCHAR* RightComment=TEXT("NeonCity.Cafe52.GroundFeet.Right");
UEdGraph* Graph(UAnimBlueprint* BP)
{
 if(BP)for(UEdGraph* G:BP->FunctionGraphs)if(G && G->GetFName()==TEXT("AnimGraph"))return G;
 return nullptr;
}
bool Binding(UAnimBlueprint* BP,UAnimBlueprint*& Original,UEdGraph*& G,UEdGraph*& OriginalGraph)
{
 if(!BP || BP->GetPathName()!=PrivatePath)return false;
 Original=LoadObject<UAnimBlueprint>(nullptr,OriginalPath);
 if(!Original || BP->ParentClass!=Original->ParentClass || !BP->ParentClass
  || !BP->ParentClass->IsChildOf(UCoastalAmbientAnimInstance::StaticClass())
  || !BP->TargetSkeleton || BP->TargetSkeleton!=Original->TargetSkeleton)return false;
 G=Graph(BP);OriginalGraph=Graph(Original);return G && OriginalGraph;
}
bool OneWire(UEdGraphPin* Input,UEdGraphNode* Source,const TCHAR* Output)
{
 return Input && Source && Input->LinkedTo.Num()==1 && Input->LinkedTo[0]==Source->FindPin(Output,EGPD_Output);
}
bool Variable(UEdGraphNode* Node,const TCHAR* Input,const TCHAR* Name)
{
 auto* Pin=Node?Node->FindPin(Input,EGPD_Input):nullptr;
 if(!Pin || Pin->LinkedTo.Num()!=1)return false;
 auto* V=Cast<UK2Node_VariableGet>(Pin->LinkedTo[0]->GetOwningNode());
 return V && V->VariableReference.GetMemberName()==Name && Pin->LinkedTo[0]==V->FindPin(Name,EGPD_Output);
}
struct FChain
{
 UAnimGraphNode_Root* Root=nullptr;
 UAnimGraphNode_ComponentToLocalSpace* ToLocal=nullptr;
 UAnimGraphNode_ModifyBone* Wrist=nullptr;
 UAnimGraphNode_TwoBoneIK* Left=nullptr;
 UAnimGraphNode_TwoBoneIK* Right=nullptr;
 int32 AmbientLayers=0;
};
bool Find(UEdGraph* G,FChain& C)
{
 int32 Roots=0,Lefts=0,Rights=0;
 if(!G)return false;
 for(UEdGraphNode* Node:G->Nodes)
 {
  if(auto* Root=Cast<UAnimGraphNode_Root>(Node)){C.Root=Root;++Roots;}
  if(Node->NodeComment==TEXT("NeonCity.C921.AmbientLayers"))++C.AmbientLayers;
  if(Node->NodeComment==LeftComment){C.Left=Cast<UAnimGraphNode_TwoBoneIK>(Node);++Lefts;}
  if(Node->NodeComment==RightComment){C.Right=Cast<UAnimGraphNode_TwoBoneIK>(Node);++Rights;}
 }
 auto* Result=C.Root?C.Root->FindPin(TEXT("Result"),EGPD_Input):nullptr;
 if(Roots!=1 || C.AmbientLayers!=1 || Lefts>1 || Rights>1 || !Result || Result->LinkedTo.Num()!=1)return false;
 C.ToLocal=Cast<UAnimGraphNode_ComponentToLocalSpace>(Result->LinkedTo[0]->GetOwningNode());
 if(!OneWire(Result,C.ToLocal,TEXT("Pose")))return false;
 auto* Component=C.ToLocal->FindPin(TEXT("ComponentPose"),EGPD_Input);
 if(!Component || Component->LinkedTo.Num()!=1)return false;
 if(C.Right)
 {
  if(!C.Left || !OneWire(Component,C.Right,TEXT("Pose"))
   || !OneWire(C.Right->FindPin(TEXT("ComponentPose"),EGPD_Input),C.Left,TEXT("Pose")))return false;
  Component=C.Left->FindPin(TEXT("ComponentPose"),EGPD_Input);
 }
 else if(C.Left || Lefts || Rights)return false;
 if(!Component || Component->LinkedTo.Num()!=1)return false;
 C.Wrist=Cast<UAnimGraphNode_ModifyBone>(Component->LinkedTo[0]->GetOwningNode());
 return C.Wrist && C.Wrist->Node.BoneToModify.BoneName==TEXT("hand_r") && OneWire(Component,C.Wrist,TEXT("Pose"));
}
bool Foot(UAnimGraphNode_TwoBoneIK* IK,const TCHAR* Bone,const TCHAR* Effector,const TCHAR* Joint)
{
 return IK && IK->Node.IKBone.BoneName==Bone && IK->Node.EffectorLocationSpace==BCS_ComponentSpace
  && IK->Node.JointTargetLocationSpace==BCS_ComponentSpace && !IK->Node.bAllowStretching
  && !IK->Node.bMaintainEffectorRelRot && !IK->Node.bTakeRotationFromEffectorSpace
  && Variable(IK,TEXT("Alpha"),TEXT("AmbientFeetAlpha"))
  && Variable(IK,TEXT("EffectorLocation"),Effector) && Variable(IK,TEXT("JointTargetLocation"),Joint);
}
}

bool UCoastalAmbientArtLibrary::AddGroundedCoastalCafeFeet(UAnimBlueprint* BP)
{
 using namespace CoastalCafeFeet52;
 UAnimBlueprint* Original=nullptr;UEdGraph* G=nullptr;UEdGraph* OriginalGraph=nullptr;FChain Before;
 if(!Binding(BP,Original,G,OriginalGraph) || !Find(G,Before) || Before.Left || Before.Right
  || G->Nodes.Num()!=OriginalGraph->Nodes.Num())return false;
 BP->Modify();G->Modify();bool OK=true;
 auto Wire=[&](UEdGraphPin* A,UEdGraphPin* B){const bool Connected=A && B && G->GetSchema()->TryCreateConnection(A,B);OK=Connected && OK;};
 auto Bind=[&](const TCHAR* Property,UEdGraphNode* To,const TCHAR* Pin)
 {
  FGraphNodeCreator<UK2Node_VariableGet> C(*G);auto* V=C.CreateNode();V->VariableReference.SetSelfMember(Property);C.Finalize();
  Wire(V->FindPin(Property,EGPD_Output),To->FindPin(Pin,EGPD_Input));
 };
 auto AddFoot=[&](const TCHAR* Bone,const TCHAR* Comment,const TCHAR* Effector,const TCHAR* Joint)
 {
  FGraphNodeCreator<UAnimGraphNode_TwoBoneIK> C(*G);auto* IK=C.CreateNode();
  IK->Node.IKBone.BoneName=Bone;IK->Node.EffectorLocationSpace=BCS_ComponentSpace;IK->Node.JointTargetLocationSpace=BCS_ComponentSpace;
  // TwoBoneIK's solver preserves the input component-space end rotation.
  // Maintaining local rotation would instead tilt the shoe with the new calf.
  IK->Node.bAllowStretching=false;IK->Node.bMaintainEffectorRelRot=false;IK->Node.bTakeRotationFromEffectorSpace=false;C.Finalize();
  for(auto& Pin:IK->ShowPinForProperties)if(Pin.PropertyName==TEXT("Alpha"))Pin.bShowPin=true;
  IK->ReconstructNode();IK->NodeComment=Comment;
  Bind(TEXT("AmbientFeetAlpha"),IK,TEXT("Alpha"));Bind(Effector,IK,TEXT("EffectorLocation"));Bind(Joint,IK,TEXT("JointTargetLocation"));return IK;
 };
 // Append only to the private clone, after the unchanged tea hand/wrist layer.
 // No original locomotion, seat, licensed activity or cup node is replaced.
 auto* Left=AddFoot(TEXT("foot_l"),LeftComment,TEXT("AmbientLeftFoot"),TEXT("AmbientLeftKnee"));
 auto* Right=AddFoot(TEXT("foot_r"),RightComment,TEXT("AmbientRightFoot"),TEXT("AmbientRightKnee"));
 auto* Input=Before.ToLocal->FindPin(TEXT("ComponentPose"),EGPD_Input);Input->BreakAllPinLinks();
 Wire(Before.Wrist->FindPin(TEXT("Pose"),EGPD_Output),Left->FindPin(TEXT("ComponentPose"),EGPD_Input));
 Wire(Left->FindPin(TEXT("Pose"),EGPD_Output),Right->FindPin(TEXT("ComponentPose"),EGPD_Input));
 Wire(Right->FindPin(TEXT("Pose"),EGPD_Output),Input);
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
 return OK && !GetGroundedCoastalCafeFeetGraphAudit(BP).IsEmpty();
}

FString UCoastalAmbientArtLibrary::GetGroundedCoastalCafeFeetGraphAudit(UAnimBlueprint* BP)
{
 using namespace CoastalCafeFeet52;
 UAnimBlueprint* Original=nullptr;UEdGraph* G=nullptr;UEdGraph* OriginalGraph=nullptr;FChain C,Old;
 if(!Binding(BP,Original,G,OriginalGraph) || !Find(G,C) || !Find(OriginalGraph,Old)
  || Old.Left || Old.Right || G->Nodes.Num()!=OriginalGraph->Nodes.Num()+8
  || !Foot(C.Left,TEXT("foot_l"),TEXT("AmbientLeftFoot"),TEXT("AmbientLeftKnee"))
  || !Foot(C.Right,TEXT("foot_r"),TEXT("AmbientRightFoot"),TEXT("AmbientRightKnee"))
  || (BP->Status!=BS_UpToDate && BP->Status!=BS_UpToDateWithWarnings) || !BP->GeneratedClass)return FString();
 // Asset/class paths cannot contain JSON quotes. This fixed schema avoids an
 // extra module dependency and reports only actual inspected graph facts.
 return FString::Printf(TEXT("{\"status\":\"CAFE52_GROUNDED_GRAPH_AUDIT\",\"blueprint\":\"%s\",\"parent_class\":\"%s\",\"skeleton\":\"%s\",\"compiled\":true,\"new_foot_ik_nodes\":2,\"original_node_count\":%d,\"private_node_count\":%d,\"original_coastal_layer_present\":true,\"output_chain_intact\":true,\"feet\":[{\"bone\":\"foot_l\",\"effector_component_space\":true,\"joint_component_space\":true,\"allow_stretching\":false,\"maintain_effector_relative_rotation\":false,\"preserve_component_effector_rotation\":true,\"alpha_variable\":\"AmbientFeetAlpha\",\"effector_variable\":\"AmbientLeftFoot\",\"joint_variable\":\"AmbientLeftKnee\"},{\"bone\":\"foot_r\",\"effector_component_space\":true,\"joint_component_space\":true,\"allow_stretching\":false,\"maintain_effector_relative_rotation\":false,\"preserve_component_effector_rotation\":true,\"alpha_variable\":\"AmbientFeetAlpha\",\"effector_variable\":\"AmbientRightFoot\",\"joint_variable\":\"AmbientRightKnee\"}]}"),
  *BP->GetPathName(),*GetPathNameSafe(BP->ParentClass),*GetPathNameSafe(BP->TargetSkeleton.Get()),OriginalGraph->Nodes.Num(),G->Nodes.Num());
}
