#include "AgentBridgeDebug.h"
#include "EngineUtils.h"
#include "UObject/UObjectIterator.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY(LogAgentBridge);

TArray<IConsoleObject*> FAgentBridgeDebug::RegisteredCommands;

void FAgentBridgeDebug::RegisterCommands()
{
	RegisteredCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("AgentBridge.DumpActor"),
		TEXT("Dump reflection data for an actor. Usage: AgentBridge.DumpActor <ActorName> [MaxDepth]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FAgentBridgeDebug::Cmd_DumpActor),
		ECVF_Default
	));

	RegisteredCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("AgentBridge.DumpClass"),
		TEXT("Dump class schema. Usage: AgentBridge.DumpClass <ClassName>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FAgentBridgeDebug::Cmd_DumpClass),
		ECVF_Default
	));

	RegisteredCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("AgentBridge.ListWorlds"),
		TEXT("List all world contexts"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&FAgentBridgeDebug::Cmd_ListWorlds),
		ECVF_Default
	));

	UE_LOG(LogAgentBridge, Log, TEXT("Debug commands registered"));
}

void FAgentBridgeDebug::UnregisterCommands()
{
	for (IConsoleObject* Cmd : RegisteredCommands)
	{
		IConsoleManager::Get().UnregisterConsoleObject(Cmd);
	}
	RegisteredCommands.Empty();
}

void FAgentBridgeDebug::Cmd_DumpActor(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogAgentBridge, Warning, TEXT("Usage: AgentBridge.DumpActor <ActorName> [MaxDepth]"));
		return;
	}

	if (!World)
	{
		UE_LOG(LogAgentBridge, Error, TEXT("No world context available"));
		return;
	}

	const FString& SearchName = Args[0];
	int32 MaxDepth = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 2;

	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor->GetName().Contains(SearchName) || Actor->GetActorLabel().Contains(SearchName))
		{
			FoundActor = Actor;
			break;
		}
	}

	if (!FoundActor)
	{
		UE_LOG(LogAgentBridge, Warning, TEXT("Actor '%s' not found in world"), *SearchName);
		return;
	}

	UE_LOG(LogAgentBridge, Log, TEXT("=== Dumping Actor: %s ==="), *FoundActor->GetName());
	UE_LOG(LogAgentBridge, Log, TEXT("  Label: %s"), *FoundActor->GetActorLabel());
	UE_LOG(LogAgentBridge, Log, TEXT("  Class: %s"), *FoundActor->GetClass()->GetName());
	UE_LOG(LogAgentBridge, Log, TEXT("  Path: %s"), *FoundActor->GetPathName());

	DumpObject(FoundActor, MaxDepth);
}

void FAgentBridgeDebug::Cmd_DumpClass(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogAgentBridge, Warning, TEXT("Usage: AgentBridge.DumpClass <ClassName>"));
		return;
	}

	const FString& SearchName = Args[0];

	// Try to find the class - handle both native and BP classes
	UClass* FoundClass = nullptr;

	// First try exact match (nullptr = search all packages)
	FoundClass = FindFirstObject<UClass>(*SearchName, EFindFirstObjectOptions::EnsureIfAmbiguous);

	// Try with _C suffix for blueprints
	if (!FoundClass && !SearchName.EndsWith(TEXT("_C")))
	{
		FoundClass = FindFirstObject<UClass>(*(SearchName + TEXT("_C")), EFindFirstObjectOptions::EnsureIfAmbiguous);
	}

	// Search all classes for partial match
	if (!FoundClass)
	{
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (It->GetName().Contains(SearchName))
			{
				FoundClass = *It;
				break;
			}
		}
	}

	if (!FoundClass)
	{
		UE_LOG(LogAgentBridge, Warning, TEXT("Class '%s' not found"), *SearchName);
		return;
	}

	DumpClassSchema(FoundClass);
}

void FAgentBridgeDebug::Cmd_ListWorlds(const TArray<FString>& Args)
{
	ListWorlds();
}

void FAgentBridgeDebug::DumpObject(UObject* Object, int32 MaxDepth)
{
	if (!Object)
	{
		UE_LOG(LogAgentBridge, Warning, TEXT("DumpObject: null object"));
		return;
	}

	UE_LOG(LogAgentBridge, Log, TEXT("--- Properties ---"));

	UClass* Class = Object->GetClass();
	for (TFieldIterator<FProperty> PropIt(Class); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Object);

		// Get clean display name (strips BP GUID suffixes)
		FString DisplayName = Property->GetAuthoredName();
		if (DisplayName.IsEmpty())
		{
			DisplayName = Property->GetName();
		}

		FString ValueStr = PropertyValueToString(Property, ValuePtr, 0, MaxDepth);
		FString TypeStr = Property->GetCPPType();

		UE_LOG(LogAgentBridge, Log, TEXT("  %s (%s) = %s"),
			*DisplayName, *TypeStr, *ValueStr);
	}
}

void FAgentBridgeDebug::DumpClassSchema(UClass* Class)
{
	if (!Class)
	{
		UE_LOG(LogAgentBridge, Warning, TEXT("DumpClassSchema: null class"));
		return;
	}

	UE_LOG(LogAgentBridge, Log, TEXT("=== Class Schema: %s ==="), *Class->GetName());
	UE_LOG(LogAgentBridge, Log, TEXT("  Path: %s"), *Class->GetPathName());
	UE_LOG(LogAgentBridge, Log, TEXT("  Parent: %s"), Class->GetSuperClass() ? *Class->GetSuperClass()->GetName() : TEXT("None"));
	UE_LOG(LogAgentBridge, Log, TEXT("  IsBlueprint: %s"), Class->HasAnyClassFlags(CLASS_CompiledFromBlueprint) ? TEXT("Yes") : TEXT("No"));
	UE_LOG(LogAgentBridge, Log, TEXT("  IsAbstract: %s"), Class->HasAnyClassFlags(CLASS_Abstract) ? TEXT("Yes") : TEXT("No"));

	// Properties
	UE_LOG(LogAgentBridge, Log, TEXT("--- Properties ---"));
	for (TFieldIterator<FProperty> PropIt(Class, EFieldIteratorFlags::ExcludeSuper); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;

		FString DisplayName = Property->GetAuthoredName();
		if (DisplayName.IsEmpty())
		{
			DisplayName = Property->GetName();
		}

		FString TypeStr = Property->GetCPPType();
		FString FlagsStr = PropertyFlagsToString(Property->PropertyFlags);

		UE_LOG(LogAgentBridge, Log, TEXT("  %s : %s [%s]"),
			*DisplayName, *TypeStr, *FlagsStr);
	}

	// Functions
	UE_LOG(LogAgentBridge, Log, TEXT("--- Functions ---"));
	for (TFieldIterator<UFunction> FuncIt(Class, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
	{
		UFunction* Function = *FuncIt;

		FString FlagsStr = FunctionFlagsToString(Function->FunctionFlags);

		// Build parameter list
		FString ParamStr;
		for (TFieldIterator<FProperty> ParamIt(Function); ParamIt; ++ParamIt)
		{
			FProperty* Param = *ParamIt;
			if (Param->HasAnyPropertyFlags(CPF_Parm) && !Param->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				if (!ParamStr.IsEmpty()) ParamStr += TEXT(", ");
				ParamStr += FString::Printf(TEXT("%s %s"), *Param->GetCPPType(), *Param->GetName());
			}
		}

		// Find return type
		FString ReturnType = TEXT("void");
		if (FProperty* ReturnProp = Function->GetReturnProperty())
		{
			ReturnType = ReturnProp->GetCPPType();
		}

		UE_LOG(LogAgentBridge, Log, TEXT("  %s %s(%s) [%s]"),
			*ReturnType, *Function->GetName(), *ParamStr, *FlagsStr);
	}
}

void FAgentBridgeDebug::ListWorlds()
{
	UE_LOG(LogAgentBridge, Log, TEXT("=== World Contexts ==="));

	if (!GEngine)
	{
		UE_LOG(LogAgentBridge, Error, TEXT("GEngine is null"));
		return;
	}

	const TIndirectArray<FWorldContext>& WorldContexts = GEngine->GetWorldContexts();
	for (int32 i = 0; i < WorldContexts.Num(); i++)
	{
		const FWorldContext& Context = WorldContexts[i];
		UWorld* World = Context.World();

		FString WorldTypeStr;
		switch (Context.WorldType)
		{
			case EWorldType::None: WorldTypeStr = TEXT("None"); break;
			case EWorldType::Game: WorldTypeStr = TEXT("Game"); break;
			case EWorldType::Editor: WorldTypeStr = TEXT("Editor"); break;
			case EWorldType::PIE: WorldTypeStr = TEXT("PIE"); break;
			case EWorldType::EditorPreview: WorldTypeStr = TEXT("EditorPreview"); break;
			case EWorldType::GamePreview: WorldTypeStr = TEXT("GamePreview"); break;
			case EWorldType::GameRPC: WorldTypeStr = TEXT("GameRPC"); break;
			case EWorldType::Inactive: WorldTypeStr = TEXT("Inactive"); break;
			default: WorldTypeStr = TEXT("Unknown"); break;
		}

		UE_LOG(LogAgentBridge, Log, TEXT("  [%d] Type: %s, World: %s, PIEInstance: %d"),
			i,
			*WorldTypeStr,
			World ? *World->GetName() : TEXT("null"),
			Context.PIEInstance);

		if (World)
		{
			UE_LOG(LogAgentBridge, Log, TEXT("      HasBegunPlay: %s, ActorCount: %d"),
				World->HasBegunPlay() ? TEXT("Yes") : TEXT("No"),
				World->GetActorCount());
		}
	}
}

FString FAgentBridgeDebug::PropertyValueToString(FProperty* Property, const void* ValuePtr, int32 Depth, int32 MaxDepth)
{
	if (!Property || !ValuePtr)
	{
		return TEXT("null");
	}

	if (Depth >= MaxDepth)
	{
		return TEXT("...");
	}

	FString Indent = FString::ChrN(Depth * 2, ' ');

	// Boolean
	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		return BoolProp->GetPropertyValue(ValuePtr) ? TEXT("true") : TEXT("false");
	}

	// Numeric types
	if (FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
	{
		if (NumProp->IsFloatingPoint())
		{
			return FString::Printf(TEXT("%f"), NumProp->GetFloatingPointPropertyValue(ValuePtr));
		}
		else if (NumProp->IsInteger())
		{
			return FString::Printf(TEXT("%lld"), NumProp->GetSignedIntPropertyValue(ValuePtr));
		}
	}

	// String types
	if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		return FString::Printf(TEXT("\"%s\""), *StrProp->GetPropertyValue(ValuePtr));
	}

	if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		return NameProp->GetPropertyValue(ValuePtr).ToString();
	}

	if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
	{
		return TextProp->GetPropertyValue(ValuePtr).ToString();
	}

	// Object reference
	if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Property))
	{
		UObject* Obj = ObjProp->GetObjectPropertyValue(ValuePtr);
		if (Obj)
		{
			return FString::Printf(TEXT("%s (%s)"), *Obj->GetName(), *Obj->GetClass()->GetName());
		}
		return TEXT("null");
	}

	// Struct
	if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		UScriptStruct* Struct = StructProp->Struct;

		// Special handling for common types
		if (Struct == TBaseStructure<FVector>::Get())
		{
			const FVector* Vec = static_cast<const FVector*>(ValuePtr);
			return Vec->ToString();
		}
		if (Struct == TBaseStructure<FRotator>::Get())
		{
			const FRotator* Rot = static_cast<const FRotator*>(ValuePtr);
			return Rot->ToString();
		}
		if (Struct == TBaseStructure<FTransform>::Get())
		{
			const FTransform* Trans = static_cast<const FTransform*>(ValuePtr);
			return Trans->ToString();
		}
		if (Struct == TBaseStructure<FColor>::Get())
		{
			const FColor* Col = static_cast<const FColor*>(ValuePtr);
			return Col->ToString();
		}
		if (Struct == TBaseStructure<FLinearColor>::Get())
		{
			const FLinearColor* Col = static_cast<const FLinearColor*>(ValuePtr);
			return Col->ToString();
		}

		// Generic struct - recurse into members
		FString Result = TEXT("{");
		bool bFirst = true;
		for (TFieldIterator<FProperty> PropIt(Struct); PropIt; ++PropIt)
		{
			if (!bFirst) Result += TEXT(", ");
			bFirst = false;

			FProperty* SubProp = *PropIt;
			const void* SubValuePtr = SubProp->ContainerPtrToValuePtr<void>(ValuePtr);
			FString SubValue = PropertyValueToString(SubProp, SubValuePtr, Depth + 1, MaxDepth);
			Result += FString::Printf(TEXT("%s=%s"), *SubProp->GetName(), *SubValue);
		}
		Result += TEXT("}");
		return Result;
	}

	// Array
	if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
	{
		FScriptArrayHelper ArrayHelper(ArrayProp, ValuePtr);
		int32 Num = ArrayHelper.Num();

		if (Num == 0) return TEXT("[]");
		if (Depth + 1 >= MaxDepth) return FString::Printf(TEXT("[%d items]"), Num);

		FString Result = TEXT("[");
		int32 ShowCount = FMath::Min(Num, 5); // Limit displayed items
		for (int32 i = 0; i < ShowCount; i++)
		{
			if (i > 0) Result += TEXT(", ");
			Result += PropertyValueToString(ArrayProp->Inner, ArrayHelper.GetRawPtr(i), Depth + 1, MaxDepth);
		}
		if (Num > ShowCount)
		{
			Result += FString::Printf(TEXT(", ... +%d more"), Num - ShowCount);
		}
		Result += TEXT("]");
		return Result;
	}

	// Map
	if (FMapProperty* MapProp = CastField<FMapProperty>(Property))
	{
		FScriptMapHelper MapHelper(MapProp, ValuePtr);
		int32 Num = MapHelper.Num();

		if (Num == 0) return TEXT("{}");
		if (Depth + 1 >= MaxDepth) return FString::Printf(TEXT("{%d entries}"), Num);

		FString Result = TEXT("{");
		int32 Count = 0;
		for (int32 i = 0; i < MapHelper.GetMaxIndex() && Count < 5; i++)
		{
			if (MapHelper.IsValidIndex(i))
			{
				if (Count > 0) Result += TEXT(", ");
				FString KeyStr = PropertyValueToString(MapProp->KeyProp, MapHelper.GetKeyPtr(i), Depth + 1, MaxDepth);
				FString ValStr = PropertyValueToString(MapProp->ValueProp, MapHelper.GetValuePtr(i), Depth + 1, MaxDepth);
				Result += FString::Printf(TEXT("%s: %s"), *KeyStr, *ValStr);
				Count++;
			}
		}
		if (Num > Count)
		{
			Result += FString::Printf(TEXT(", ... +%d more"), Num - Count);
		}
		Result += TEXT("}");
		return Result;
	}

	// Enum
	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		UEnum* Enum = EnumProp->GetEnum();
		FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
		int64 Value = UnderlyingProp->GetSignedIntPropertyValue(ValuePtr);
		FString EnumName = Enum->GetNameStringByValue(Value);
		return FString::Printf(TEXT("%s (%lld)"), *EnumName, Value);
	}

	if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
	{
		if (UEnum* Enum = ByteProp->Enum)
		{
			uint8 Value = ByteProp->GetPropertyValue(ValuePtr);
			FString EnumName = Enum->GetNameStringByValue(Value);
			return FString::Printf(TEXT("%s (%d)"), *EnumName, Value);
		}
		return FString::Printf(TEXT("%d"), ByteProp->GetPropertyValue(ValuePtr));
	}

	// Fallback - use ExportText
	FString ExportedValue;
	Property->ExportTextItem_Direct(ExportedValue, ValuePtr, nullptr, nullptr, PPF_None);
	if (ExportedValue.Len() > 100)
	{
		ExportedValue = ExportedValue.Left(100) + TEXT("...");
	}
	return ExportedValue;
}

FString FAgentBridgeDebug::PropertyFlagsToString(EPropertyFlags Flags)
{
	TArray<FString> FlagStrs;

	if (Flags & CPF_Edit) FlagStrs.Add(TEXT("Edit"));
	if (Flags & CPF_BlueprintVisible) FlagStrs.Add(TEXT("BPVisible"));
	if (Flags & CPF_BlueprintReadOnly) FlagStrs.Add(TEXT("BPReadOnly"));
	if (Flags & CPF_Net) FlagStrs.Add(TEXT("Net"));
	if (Flags & CPF_SaveGame) FlagStrs.Add(TEXT("SaveGame"));
	if (Flags & CPF_EditConst) FlagStrs.Add(TEXT("EditConst"));
	if (Flags & CPF_Transient) FlagStrs.Add(TEXT("Transient"));
	if (Flags & CPF_Config) FlagStrs.Add(TEXT("Config"));

	return FString::Join(FlagStrs, TEXT(", "));
}

FString FAgentBridgeDebug::FunctionFlagsToString(EFunctionFlags Flags)
{
	TArray<FString> FlagStrs;

	if (Flags & FUNC_BlueprintCallable) FlagStrs.Add(TEXT("BPCallable"));
	if (Flags & FUNC_BlueprintPure) FlagStrs.Add(TEXT("Pure"));
	if (Flags & FUNC_Native) FlagStrs.Add(TEXT("Native"));
	if (Flags & FUNC_Event) FlagStrs.Add(TEXT("Event"));
	if (Flags & FUNC_Net) FlagStrs.Add(TEXT("Net"));
	if (Flags & FUNC_Static) FlagStrs.Add(TEXT("Static"));
	if (Flags & FUNC_Exec) FlagStrs.Add(TEXT("Exec"));

	return FString::Join(FlagStrs, TEXT(", "));
}
