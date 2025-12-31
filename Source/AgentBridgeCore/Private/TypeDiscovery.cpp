#include "TypeDiscovery.h"
#include "PropertyAccessor.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "StructUtils/UserDefinedStruct.h"
#include "Engine/UserDefinedEnum.h"

//~==============================================================================
// Class Discovery
//~==============================================================================

UClass* FTypeDiscovery::FindClassByName(const FString& ClassName)
{
	if (ClassName.IsEmpty())
	{
		return nullptr;
	}

	FString SearchName = NormalizeClassName(ClassName);
	UClass* FoundClass = nullptr;

	// Try 1: Exact match using FindFirstObject (modern UE5 API)
	FoundClass = FindFirstObject<UClass>(*SearchName, EFindFirstObjectOptions::EnsureIfAmbiguous);
	if (FoundClass)
	{
		return FoundClass;
	}

	// Try 2: With _C suffix for Blueprint classes
	if (!SearchName.EndsWith(TEXT("_C")))
	{
		FString BPName = SearchName + TEXT("_C");
		FoundClass = FindFirstObject<UClass>(*BPName, EFindFirstObjectOptions::EnsureIfAmbiguous);
		if (FoundClass)
		{
			return FoundClass;
		}
	}

	// Try 3: Search all classes for partial match
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		FString IterClassName = Class->GetName();

		// Check for exact name match (ignoring path)
		if (IterClassName.Equals(SearchName, ESearchCase::IgnoreCase))
		{
			return Class;
		}

		// Check with _C suffix
		if (!SearchName.EndsWith(TEXT("_C")) &&
			IterClassName.Equals(SearchName + TEXT("_C"), ESearchCase::IgnoreCase))
		{
			return Class;
		}
	}

	// Try 4: Load from path if it looks like a path
	if (SearchName.Contains(TEXT("/")))
	{
		FoundClass = LoadClass<UObject>(nullptr, *SearchName);
		if (FoundClass)
		{
			return FoundClass;
		}
	}

	return nullptr;
}

TArray<UClass*> FTypeDiscovery::GetAllClassesOfType(
	UClass* BaseClass,
	bool bIncludeAbstract,
	bool bBlueprintOnly)
{
	TArray<UClass*> Results;

	if (!BaseClass)
	{
		BaseClass = UObject::StaticClass();
	}

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;

		// Must be a subclass of base
		if (!Class->IsChildOf(BaseClass))
		{
			continue;
		}

		// Skip abstract if requested
		if (!bIncludeAbstract && Class->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}

		// Filter for Blueprint-only if requested
		if (bBlueprintOnly && !IsBlueprintClass(Class))
		{
			continue;
		}

		// Skip deprecated classes
		if (Class->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			continue;
		}

		Results.Add(Class);
	}

	return Results;
}

FAgentClassInfo FTypeDiscovery::GetClassInfo(UClass* Class)
{
	FAgentClassInfo Info;

	if (!Class)
	{
		return Info;
	}

	Info.ClassName = Class->GetName();
	Info.DisplayName = GetDisplayClassName(Class);
	Info.ClassPath = GetClassPath(Class);
	Info.bIsBlueprintClass = IsBlueprintClass(Class);
	Info.bIsAbstract = Class->HasAnyClassFlags(CLASS_Abstract);

	// Parent class
	if (UClass* ParentClass = Class->GetSuperClass())
	{
		Info.ParentClassName = ParentClass->GetName();
	}

	// Implemented interfaces
	for (const FImplementedInterface& Interface : Class->Interfaces)
	{
		if (Interface.Class)
		{
			Info.ImplementedInterfaces.Add(Interface.Class->GetName());
		}
	}

	return Info;
}

TArray<FAgentPropertyInfo> FTypeDiscovery::GetClassProperties(
	UClass* Class,
	bool bIncludeParents,
	bool bIncludeHidden)
{
	TArray<FAgentPropertyInfo> Results;

	if (!Class)
	{
		return Results;
	}

	EFieldIteratorFlags::SuperClassFlags SuperFlags =
		bIncludeParents ? EFieldIteratorFlags::IncludeSuper : EFieldIteratorFlags::ExcludeSuper;

	for (TFieldIterator<FProperty> PropIt(Class, SuperFlags); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;

		// Skip hidden properties unless requested
		if (!bIncludeHidden)
		{
			// Check if exposed to Blueprints or editor
			bool bExposed =
				Property->HasAnyPropertyFlags(CPF_BlueprintVisible | CPF_Edit) ||
				!Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance);

			if (!bExposed)
			{
				continue;
			}
		}

		Results.Add(BuildPropertyInfo(Property));
	}

	return Results;
}

TArray<FAgentFunctionSignature> FTypeDiscovery::GetClassFunctions(
	UClass* Class,
	bool bIncludeParents,
	bool bBlueprintOnly)
{
	TArray<FAgentFunctionSignature> Results;

	if (!Class)
	{
		return Results;
	}

	EFieldIteratorFlags::SuperClassFlags SuperFlags =
		bIncludeParents ? EFieldIteratorFlags::IncludeSuper : EFieldIteratorFlags::ExcludeSuper;

	for (TFieldIterator<UFunction> FuncIt(Class, SuperFlags); FuncIt; ++FuncIt)
	{
		UFunction* Function = *FuncIt;

		// Filter for Blueprint-callable if requested
		if (bBlueprintOnly && !Function->HasAnyFunctionFlags(FUNC_BlueprintCallable))
		{
			continue;
		}

		// Skip internal/hidden functions
		if (Function->HasAnyFunctionFlags(FUNC_EditorOnly) && !GIsEditor)
		{
			continue;
		}

		Results.Add(BuildFunctionSignature(Function));
	}

	return Results;
}

//~==============================================================================
// Struct Discovery
//~==============================================================================

UScriptStruct* FTypeDiscovery::FindStructByName(const FString& StructName)
{
	if (StructName.IsEmpty())
	{
		return nullptr;
	}

	// Try exact match first
	UScriptStruct* FoundStruct = FindFirstObject<UScriptStruct>(*StructName, EFindFirstObjectOptions::EnsureIfAmbiguous);
	if (FoundStruct)
	{
		return FoundStruct;
	}

	// Search all structs
	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		if (It->GetName().Equals(StructName, ESearchCase::IgnoreCase))
		{
			return *It;
		}
	}

	return nullptr;
}

TArray<FAgentPropertyInfo> FTypeDiscovery::GetStructProperties(UScriptStruct* Struct)
{
	TArray<FAgentPropertyInfo> Results;

	if (!Struct)
	{
		return Results;
	}

	for (TFieldIterator<FProperty> PropIt(Struct); PropIt; ++PropIt)
	{
		Results.Add(BuildPropertyInfo(*PropIt));
	}

	return Results;
}

bool FTypeDiscovery::IsUserDefinedStruct(UScriptStruct* Struct)
{
	if (!Struct)
	{
		return false;
	}

	return Struct->IsA<UUserDefinedStruct>();
}

//~==============================================================================
// Enum Discovery
//~==============================================================================

UEnum* FTypeDiscovery::FindEnumByName(const FString& EnumName)
{
	if (EnumName.IsEmpty())
	{
		return nullptr;
	}

	// Try exact match
	UEnum* FoundEnum = FindFirstObject<UEnum>(*EnumName, EFindFirstObjectOptions::EnsureIfAmbiguous);
	if (FoundEnum)
	{
		return FoundEnum;
	}

	// Search all enums
	for (TObjectIterator<UEnum> It; It; ++It)
	{
		if (It->GetName().Equals(EnumName, ESearchCase::IgnoreCase))
		{
			return *It;
		}
	}

	return nullptr;
}

TArray<FAgentEnumValue> FTypeDiscovery::GetEnumValues(UEnum* Enum)
{
	TArray<FAgentEnumValue> Results;

	if (!Enum)
	{
		return Results;
	}

	// Get the number of enum values (excluding _MAX if present)
	int32 NumEnums = Enum->NumEnums();

	// Check if last entry is the auto-generated _MAX
	bool bHasMax = false;
	if (NumEnums > 0)
	{
		FString LastName = Enum->GetNameStringByIndex(NumEnums - 1);
		bHasMax = LastName.EndsWith(TEXT("_MAX"));
	}

	int32 Count = bHasMax ? NumEnums - 1 : NumEnums;

	for (int32 i = 0; i < Count; i++)
	{
		FAgentEnumValue EnumValue;
		EnumValue.Name = Enum->GetNameStringByIndex(i);
		EnumValue.DisplayName = Enum->GetDisplayNameTextByIndex(i).ToString();
		EnumValue.Value = Enum->GetValueByIndex(i);

		// Clean up the name (remove enum prefix if present)
		int32 ColonPos;
		if (EnumValue.Name.FindLastChar(':', ColonPos))
		{
			EnumValue.Name = EnumValue.Name.Mid(ColonPos + 1);
		}

		Results.Add(EnumValue);
	}

	return Results;
}

bool FTypeDiscovery::IsUserDefinedEnum(UEnum* Enum)
{
	if (!Enum)
	{
		return false;
	}

	return Enum->IsA<UUserDefinedEnum>();
}

//~==============================================================================
// Name Normalization Utilities
//~==============================================================================

FString FTypeDiscovery::GetDisplayClassName(UClass* Class)
{
	if (!Class)
	{
		return TEXT("");
	}

	FString Name = Class->GetName();

	// Remove _C suffix from Blueprint classes
	if (IsBlueprintClass(Class) && Name.EndsWith(TEXT("_C")))
	{
		Name = Name.LeftChop(2);
	}

	return Name;
}

FString FTypeDiscovery::GetDisplayPropertyName(FProperty* Property)
{
	if (!Property)
	{
		return TEXT("");
	}

	// GetAuthoredName() returns the clean name for BP properties
	FString AuthoredName = Property->GetAuthoredName();
	if (!AuthoredName.IsEmpty())
	{
		return AuthoredName;
	}

	return Property->GetName();
}

FString FTypeDiscovery::NormalizeClassName(const FString& Input)
{
	FString Result = Input;

	// Trim whitespace
	Result.TrimStartAndEndInline();

	// If it's a full path, return as-is
	if (Result.Contains(TEXT("/")))
	{
		return Result;
	}

	// Remove common prefixes for lookup (A for Actor, U for UObject derivatives)
	// But keep the prefix if the class is actually named with it
	// We'll try both with and without prefix in FindClassByName

	return Result;
}

bool FTypeDiscovery::IsBlueprintClass(UClass* Class)
{
	if (!Class)
	{
		return false;
	}

	return Class->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
}

FString FTypeDiscovery::GetClassPath(UClass* Class)
{
	if (!Class)
	{
		return TEXT("");
	}

	return Class->GetPathName();
}

//~==============================================================================
// Private Helpers
//~==============================================================================

FAgentPropertyInfo FTypeDiscovery::BuildPropertyInfo(FProperty* Property)
{
	FAgentPropertyInfo Info;

	if (!Property)
	{
		return Info;
	}

	Info.PropertyName = Property->GetName();
	Info.DisplayName = GetDisplayPropertyName(Property);
	Info.Type = FPropertyAccessor::GetPropertyType(Property);
	Info.TypeName = FPropertyAccessor::GetPropertyTypeName(Property);
	Info.bIsReadOnly = !FPropertyAccessor::IsPropertyWritable(Property);

	// Check for editor-only
	Info.bIsEditorOnly = Property->HasAnyPropertyFlags(CPF_EditorOnly);

	// Get category metadata (editor-only, will be empty in packaged builds)
#if WITH_EDITORONLY_DATA
	Info.Category = Property->GetMetaData(TEXT("Category"));
	Info.Description = Property->GetToolTipText().ToString();
#endif

	return Info;
}

FAgentFunctionSignature FTypeDiscovery::BuildFunctionSignature(UFunction* Function)
{
	FAgentFunctionSignature Sig;

	if (!Function)
	{
		return Sig;
	}

	Sig.FunctionName = Function->GetName();
	Sig.bIsStatic = Function->HasAnyFunctionFlags(FUNC_Static);
	Sig.bIsBlueprintCallable = Function->HasAnyFunctionFlags(FUNC_BlueprintCallable);

	// Check for world context parameter (hidden in BP)
	Sig.bNeedsWorldContext = false;
	FString WorldContextMeta = Function->GetMetaData(TEXT("WorldContext"));
	if (!WorldContextMeta.IsEmpty())
	{
		Sig.bNeedsWorldContext = true;
	}

#if WITH_EDITORONLY_DATA
	Sig.Description = Function->GetToolTipText().ToString();
#endif

	// Build parameter list
	for (TFieldIterator<FProperty> ParamIt(Function); ParamIt; ++ParamIt)
	{
		FProperty* Param = *ParamIt;

		// Skip return parameter for now
		if (Param->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			Sig.ReturnValue = BuildPropertyInfo(Param);
			continue;
		}

		// Only include actual parameters
		if (Param->HasAnyPropertyFlags(CPF_Parm))
		{
			// Skip hidden world context parameter
			if (Param->GetName() == WorldContextMeta)
			{
				continue;
			}

			Sig.Parameters.Add(BuildPropertyInfo(Param));
		}
	}

	return Sig;
}
