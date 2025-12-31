#include "AgentBridgeDebug.h"
#include "AgentBridgeTypes.h"
#include "AgentPropertyPath.h"
#include "ActorOperations.h"
#include "FunctionInvoker.h"
#include "WorldContextManager.h"
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

	// PropertyPath commands
	RegisteredCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("AgentBridge.GetPath"),
		TEXT("Read nested property path. Usage: AgentBridge.GetPath <ActorName> <Path>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FAgentBridgeDebug::Cmd_GetPath),
		ECVF_Default
	));

	RegisteredCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("AgentBridge.SetPath"),
		TEXT("Write to property path. Usage: AgentBridge.SetPath <ActorName> <Path> <Value>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FAgentBridgeDebug::Cmd_SetPath),
		ECVF_Default
	));

	// ActorOperations commands
	RegisteredCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("AgentBridge.QueryActors"),
		TEXT("Query actors by pattern. Usage: AgentBridge.QueryActors [Pattern] [Limit]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FAgentBridgeDebug::Cmd_QueryActors),
		ECVF_Default
	));

	RegisteredCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("AgentBridge.SpawnActor"),
		TEXT("Spawn actor. Usage: AgentBridge.SpawnActor <Class> [X Y Z] [Label]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FAgentBridgeDebug::Cmd_SpawnActor),
		ECVF_Default
	));

	// FunctionInvoker commands
	RegisteredCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("AgentBridge.CallFunc"),
		TEXT("Call function on actor. Usage: AgentBridge.CallFunc <ActorName> <FunctionName>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FAgentBridgeDebug::Cmd_CallFunc),
		ECVF_Default
	));

	UE_LOG(LogAgentBridge, Log, TEXT("Debug commands registered (8 commands)"));
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

//~==============================================================================
// Helper Functions
//~==============================================================================

AActor* FAgentBridgeDebug::FindActorByName(UWorld* World, const FString& SearchName)
{
	if (!World || SearchName.IsEmpty())
	{
		return nullptr;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor->GetName().Contains(SearchName) || Actor->GetActorLabel().Contains(SearchName))
		{
			return Actor;
		}
	}

	return nullptr;
}

FString FAgentBridgeDebug::AgentValueToString(const FAgentPropertyValue& Value)
{
	switch (Value.Type)
	{
	case EAgentPropertyType::None:
		return TEXT("(none)");

	case EAgentPropertyType::Bool:
		return Value.GetBool() ? TEXT("true") : TEXT("false");

	case EAgentPropertyType::Int8:
	case EAgentPropertyType::Int16:
	case EAgentPropertyType::Int32:
	case EAgentPropertyType::Int64:
	case EAgentPropertyType::UInt8:
	case EAgentPropertyType::UInt16:
	case EAgentPropertyType::UInt32:
	case EAgentPropertyType::UInt64:
		return FString::Printf(TEXT("%lld"), Value.GetInt());

	case EAgentPropertyType::Float:
	case EAgentPropertyType::Double:
		return FString::Printf(TEXT("%f"), Value.GetFloat());

	case EAgentPropertyType::String:
	case EAgentPropertyType::Text:
		return FString::Printf(TEXT("\"%s\""), *Value.GetString());

	case EAgentPropertyType::Name:
		return Value.GetString();

	case EAgentPropertyType::Vector:
	{
		FVector V = Value.AsVector();
		return FString::Printf(TEXT("(X=%f, Y=%f, Z=%f)"), V.X, V.Y, V.Z);
	}

	case EAgentPropertyType::Rotator:
	{
		FRotator R = Value.AsRotator();
		return FString::Printf(TEXT("(P=%f, Y=%f, R=%f)"), R.Pitch, R.Yaw, R.Roll);
	}

	case EAgentPropertyType::Transform:
	{
		FTransform T = Value.AsTransform();
		return T.ToString();
	}

	case EAgentPropertyType::Color:
	{
		FColor C = FColor(
			static_cast<uint8>(Value.StructValue.Contains(TEXT("R")) ? Value.StructValue[TEXT("R")]->GetInt() : 0),
			static_cast<uint8>(Value.StructValue.Contains(TEXT("G")) ? Value.StructValue[TEXT("G")]->GetInt() : 0),
			static_cast<uint8>(Value.StructValue.Contains(TEXT("B")) ? Value.StructValue[TEXT("B")]->GetInt() : 0),
			static_cast<uint8>(Value.StructValue.Contains(TEXT("A")) ? Value.StructValue[TEXT("A")]->GetInt() : 255)
		);
		return C.ToString();
	}

	case EAgentPropertyType::Object:
	case EAgentPropertyType::SoftObject:
	case EAgentPropertyType::WeakObject:
		return FString::Printf(TEXT("Object: %s"), *Value.GetString());

	case EAgentPropertyType::Class:
		return FString::Printf(TEXT("Class: %s"), *Value.GetString());

	case EAgentPropertyType::Array:
		return FString::Printf(TEXT("[Array with %d elements]"), Value.ArrayValue.Num());

	case EAgentPropertyType::Map:
	case EAgentPropertyType::Set:
		return FString::Printf(TEXT("{Map with %d entries}"), Value.StructValue.Num());

	case EAgentPropertyType::Struct:
	{
		FString Result = TEXT("{");
		bool bFirst = true;
		for (const auto& Pair : Value.StructValue)
		{
			if (!bFirst) Result += TEXT(", ");
			bFirst = false;
			Result += FString::Printf(TEXT("%s=%s"), *Pair.Key,
				Pair.Value.IsValid() ? *AgentValueToString(*Pair.Value) : TEXT("null"));
		}
		Result += TEXT("}");
		return Result;
	}

	case EAgentPropertyType::Enum:
		return Value.GetString();

	case EAgentPropertyType::Unknown:
	default:
		return TEXT("(unknown type)");
	}
}

//~==============================================================================
// PropertyPath Commands
//~==============================================================================

void FAgentBridgeDebug::Cmd_GetPath(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 2)
	{
		UE_LOG(LogAgentBridge, Warning, TEXT("Usage: AgentBridge.GetPath <ActorName> <Path>"));
		UE_LOG(LogAgentBridge, Warning, TEXT("  Example: AgentBridge.GetPath Floor RelativeLocation.X"));
		return;
	}

	if (!World)
	{
		World = FWorldContextManager::Get().GetTargetWorld();
	}

	if (!World)
	{
		UE_LOG(LogAgentBridge, Error, TEXT("No world context available"));
		return;
	}

	AActor* Actor = FindActorByName(World, Args[0]);
	if (!Actor)
	{
		UE_LOG(LogAgentBridge, Warning, TEXT("Actor '%s' not found"), *Args[0]);
		return;
	}

	const FString& Path = Args[1];

	UE_LOG(LogAgentBridge, Log, TEXT("=== GetPath: %s.%s ==="), *Actor->GetName(), *Path);

	FPropertyPathResult Result = FAgentPropertyPath::GetValue(Actor, Path);

	if (Result.bSuccess)
	{
		UE_LOG(LogAgentBridge, Log, TEXT("  Value: %s"), *AgentValueToString(Result.Value));
		UE_LOG(LogAgentBridge, Log, TEXT("  Type: %d"), static_cast<int32>(Result.Value.Type));
	}
	else
	{
		UE_LOG(LogAgentBridge, Error, TEXT("  Error: %s"), *Result.ErrorMessage);
	}
}

void FAgentBridgeDebug::Cmd_SetPath(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 3)
	{
		UE_LOG(LogAgentBridge, Warning, TEXT("Usage: AgentBridge.SetPath <ActorName> <Path> <Value>"));
		UE_LOG(LogAgentBridge, Warning, TEXT("  Example: AgentBridge.SetPath Floor RelativeScale3D.X 2.0"));
		return;
	}

	if (!World)
	{
		World = FWorldContextManager::Get().GetTargetWorld();
	}

	if (!World)
	{
		UE_LOG(LogAgentBridge, Error, TEXT("No world context available"));
		return;
	}

	AActor* Actor = FindActorByName(World, Args[0]);
	if (!Actor)
	{
		UE_LOG(LogAgentBridge, Warning, TEXT("Actor '%s' not found"), *Args[0]);
		return;
	}

	const FString& Path = Args[1];
	const FString& ValueStr = Args[2];

	UE_LOG(LogAgentBridge, Log, TEXT("=== SetPath: %s.%s = %s ==="), *Actor->GetName(), *Path, *ValueStr);

	// First get the current value to determine the type
	FPropertyPathResult CurrentResult = FAgentPropertyPath::GetValue(Actor, Path);
	if (!CurrentResult.bSuccess)
	{
		UE_LOG(LogAgentBridge, Error, TEXT("  Path error: %s"), *CurrentResult.ErrorMessage);
		return;
	}

	// Create value of the same type
	FAgentPropertyValue NewValue;
	switch (CurrentResult.Value.Type)
	{
	case EAgentPropertyType::Bool:
		NewValue = FAgentPropertyValue(ValueStr.Equals(TEXT("true"), ESearchCase::IgnoreCase) ||
			ValueStr.Equals(TEXT("1")));
		break;

	case EAgentPropertyType::Int8:
	case EAgentPropertyType::Int16:
	case EAgentPropertyType::Int32:
	case EAgentPropertyType::Int64:
	case EAgentPropertyType::UInt8:
	case EAgentPropertyType::UInt16:
	case EAgentPropertyType::UInt32:
	case EAgentPropertyType::UInt64:
		NewValue = FAgentPropertyValue(static_cast<int64>(FCString::Atoi64(*ValueStr)));
		break;

	case EAgentPropertyType::Float:
	case EAgentPropertyType::Double:
		NewValue = FAgentPropertyValue(FCString::Atod(*ValueStr));
		break;

	case EAgentPropertyType::String:
	case EAgentPropertyType::Name:
	case EAgentPropertyType::Text:
		NewValue = FAgentPropertyValue(ValueStr);
		break;

	default:
		UE_LOG(LogAgentBridge, Error, TEXT("  Cannot set value of type %d via console"), static_cast<int32>(CurrentResult.Value.Type));
		return;
	}

	bool bSuccess = FAgentPropertyPath::SetValue(Actor, Path, NewValue);

	if (bSuccess)
	{
		UE_LOG(LogAgentBridge, Log, TEXT("  Success! New value set."));

		// Verify by reading back
		FPropertyPathResult VerifyResult = FAgentPropertyPath::GetValue(Actor, Path);
		if (VerifyResult.bSuccess)
		{
			UE_LOG(LogAgentBridge, Log, TEXT("  Verified: %s"), *AgentValueToString(VerifyResult.Value));
		}
	}
	else
	{
		UE_LOG(LogAgentBridge, Error, TEXT("  Failed to set value"));
	}
}

//~==============================================================================
// ActorOperations Commands
//~==============================================================================

void FAgentBridgeDebug::Cmd_QueryActors(const TArray<FString>& Args, UWorld* World)
{
	if (!World)
	{
		World = FWorldContextManager::Get().GetTargetWorld();
	}

	if (!World)
	{
		UE_LOG(LogAgentBridge, Error, TEXT("No world context available"));
		return;
	}

	FActorQueryParams Params;
	Params.Limit = 20; // Default limit for console output

	if (Args.Num() > 0)
	{
		Params.NamePattern = Args[0];
	}

	if (Args.Num() > 1)
	{
		Params.Limit = FCString::Atoi(*Args[1]);
	}

	UE_LOG(LogAgentBridge, Log, TEXT("=== QueryActors (Pattern='%s', Limit=%d) ==="),
		*Params.NamePattern, Params.Limit);

	TArray<FActorReference> Results = FActorOperations::QueryActors(Params, World);

	UE_LOG(LogAgentBridge, Log, TEXT("Found %d actors:"), Results.Num());

	for (int32 i = 0; i < Results.Num(); i++)
	{
		const FActorReference& Ref = Results[i];
		UE_LOG(LogAgentBridge, Log, TEXT("  [%d] %s (%s) - Label: %s"),
			i, *Ref.Name, *Ref.ClassName, *Ref.Label);
	}
}

void FAgentBridgeDebug::Cmd_SpawnActor(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogAgentBridge, Warning, TEXT("Usage: AgentBridge.SpawnActor <Class> [X Y Z] [Label]"));
		UE_LOG(LogAgentBridge, Warning, TEXT("  Example: AgentBridge.SpawnActor StaticMeshActor 0 0 100 MyActor"));
		return;
	}

	if (!World)
	{
		World = FWorldContextManager::Get().GetTargetWorld();
	}

	if (!World)
	{
		UE_LOG(LogAgentBridge, Error, TEXT("No world context available"));
		return;
	}

	FActorSpawnParams Params;
	Params.ClassPath = Args[0];

	// Parse location if provided
	if (Args.Num() >= 4)
	{
		float X = FCString::Atof(*Args[1]);
		float Y = FCString::Atof(*Args[2]);
		float Z = FCString::Atof(*Args[3]);
		Params.Transform.SetLocation(FVector(X, Y, Z));
	}

	// Parse label if provided
	if (Args.Num() >= 5)
	{
		Params.ActorLabel = Args[4];
	}

	UE_LOG(LogAgentBridge, Log, TEXT("=== SpawnActor ==="));
	UE_LOG(LogAgentBridge, Log, TEXT("  Class: %s"), *Params.ClassPath);
	UE_LOG(LogAgentBridge, Log, TEXT("  Location: %s"), *Params.Transform.GetLocation().ToString());
	UE_LOG(LogAgentBridge, Log, TEXT("  Label: %s"), *Params.ActorLabel);

	FString Error;
	AActor* NewActor = FActorOperations::SpawnActor(Params, World, &Error);

	if (NewActor)
	{
		UE_LOG(LogAgentBridge, Log, TEXT("  Success! Spawned: %s"), *NewActor->GetName());
		UE_LOG(LogAgentBridge, Log, TEXT("  Path: %s"), *NewActor->GetPathName());
	}
	else
	{
		UE_LOG(LogAgentBridge, Error, TEXT("  Failed: %s"), *Error);
	}
}

//~==============================================================================
// FunctionInvoker Commands
//~==============================================================================

void FAgentBridgeDebug::Cmd_CallFunc(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 2)
	{
		UE_LOG(LogAgentBridge, Warning, TEXT("Usage: AgentBridge.CallFunc <ActorName> <FunctionName>"));
		UE_LOG(LogAgentBridge, Warning, TEXT("  Example: AgentBridge.CallFunc MyActor K2_GetActorLocation"));
		return;
	}

	if (!World)
	{
		World = FWorldContextManager::Get().GetTargetWorld();
	}

	if (!World)
	{
		UE_LOG(LogAgentBridge, Error, TEXT("No world context available"));
		return;
	}

	AActor* Actor = FindActorByName(World, Args[0]);
	if (!Actor)
	{
		UE_LOG(LogAgentBridge, Warning, TEXT("Actor '%s' not found"), *Args[0]);
		return;
	}

	const FString& FunctionName = Args[1];

	UE_LOG(LogAgentBridge, Log, TEXT("=== CallFunc: %s.%s() ==="), *Actor->GetName(), *FunctionName);

	// Find the function
	UFunction* Function = FFunctionInvoker::FindFunction(Actor->GetClass(), FunctionName);
	if (!Function)
	{
		UE_LOG(LogAgentBridge, Error, TEXT("  Function '%s' not found on %s"), *FunctionName, *Actor->GetClass()->GetName());

		// List available functions
		TArray<UFunction*> Funcs = FFunctionInvoker::GetCallableFunctions(Actor->GetClass(), false, true);
		if (Funcs.Num() > 0)
		{
			UE_LOG(LogAgentBridge, Log, TEXT("  Available BP-callable functions:"));
			for (int32 i = 0; i < FMath::Min(10, Funcs.Num()); i++)
			{
				UE_LOG(LogAgentBridge, Log, TEXT("    - %s"), *Funcs[i]->GetName());
			}
			if (Funcs.Num() > 10)
			{
				UE_LOG(LogAgentBridge, Log, TEXT("    ... and %d more"), Funcs.Num() - 10);
			}
		}
		return;
	}

	// Get function signature
	FAgentFunctionSignature Sig = FFunctionInvoker::GetFunctionSignature(Function);
	UE_LOG(LogAgentBridge, Log, TEXT("  Signature: %d params, returns: %s"),
		Sig.Parameters.Num(),
		Sig.ReturnValue.PropertyName.IsEmpty() ? TEXT("void") : *Sig.ReturnValue.TypeName);

	// Invoke with no parameters (for simple functions)
	TMap<FString, FAgentPropertyValue> EmptyParams;
	FAgentFunctionResult Result = FFunctionInvoker::InvokeFunction(Actor, Function, EmptyParams, World);

	if (Result.bSuccess)
	{
		UE_LOG(LogAgentBridge, Log, TEXT("  Success!"));

		if (Result.ReturnValue.Type != EAgentPropertyType::None)
		{
			UE_LOG(LogAgentBridge, Log, TEXT("  Return: %s"), *AgentValueToString(Result.ReturnValue));
		}

		for (const auto& Pair : Result.OutParams)
		{
			if (Pair.Value.IsValid())
			{
				UE_LOG(LogAgentBridge, Log, TEXT("  Out[%s]: %s"), *Pair.Key, *AgentValueToString(*Pair.Value));
			}
		}
	}
	else
	{
		UE_LOG(LogAgentBridge, Error, TEXT("  Error: %s"), *Result.ErrorMessage);
	}
}
