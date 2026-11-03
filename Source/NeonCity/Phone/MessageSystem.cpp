#include "Phone/MessageSystem.h"
FGuid UMessageSystem::SendMessage(FName Sender, FText Content, FName ActionId)
{
	if (Sender.IsNone() || Content.IsEmpty()) { return FGuid(); }
	FPhoneMessage Message;
	Message.Id=FGuid::NewGuid(); Message.Sender=Sender; Message.Content=Content;
	Message.Timestamp=FDateTime::UtcNow(); Message.ActionId=ActionId;
	if (Messages.Num() >= 100) { Messages.RemoveAt(0); }
	Messages.Add(Message);
	OnInboxChanged.Broadcast();
	return Message.Id;
}
