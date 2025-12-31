#include "FunctionInvoker.h"
#include "PropertyAccessor.h"
#include "TypeDiscovery.h"
#include "UObject/UnrealType.h"

//~==============================================================================
// Function Discovery
//~==============================================================================

TArray<UFunction*> FFunctionInvoker::GetCallableFunctions(
	UClass* Class,
	bool bIncludeParents,
	bool bBlueprintOnly)
{
	TArray<UFunction*> Results;

	if (!Class)
	{
		return Results;
	}

	EFieldIteratorFlags::SuperClassFlags SuperFlags =
		bIncludeParents ? EFieldIteratorFlags::IncludeSuper : EFieldIteratorFlags::ExcludeSuper;

	for (TFieldIterator<UFunction> FuncIt(Class, SuperFlags); FuncIt; ++FuncIt)
	{
		UFunction* Function = *FuncIt;

		// Filter based on callable flags
		bool bIsCallable =
			Function->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_Exec | FUNC_Event);

		if (!bIsCallable)
		{
			continue;
		}

		// Filter for Blueprint-only if requested
		if (bBlueprintOnly && !Function->HasAnyFunctionFlags(FUNC_BlueprintCallable))
		{
			continue;
		}

		// Skip delegate signature functions
		if (Function->HasAnyFunctionFlags(FUNC_Delegate))
		{
			continue;
		}

		Results.Add(Function);
	}

	return Results;
}

UFunction* FFunctionInvoker::FindFunction(UClass* Class, const FString& FunctionName)
{
	if (!Class || FunctionName.IsEmpty())
	{
		return nullptr;
	}

	// Try direct name lookup first
	UFunction* Function = Class->FindFunctionByName(*FunctionName);
	if (Function)
	{
		return Function;
	}

	// Search all functions for case-insensitive match
	for (TFieldIterator<UFunction> FuncIt(Class); FuncIt; ++FuncIt)
	{
		if (FuncIt->GetName().Equals(FunctionName, ESearchCase::IgnoreCase))
		{
			return *FuncIt;
		}
	}

	return nullptr;
}

FAgentFunctionSignature FFunctionInvoker::GetFunctionSignature(UFunction* Function)
{
	FAgentFunctionSignature Sig;

	if (!Function)
	{
		return Sig;
	}

	Sig.FunctionName = Function->GetName();
	Sig.bIsStatic = Function->HasAnyFunctionFlags(FUNC_Static);
	Sig.bIsBlueprintCallable = Function->HasAnyFunctionFlags(FUNC_BlueprintCallable);
	Sig.bNeedsWorldContext = FunctionNeedsWorldContext(Function);

#if WITH_EDITORONLY_DATA
	Sig.Description = Function->GetToolTipText().ToString();
#endif

	// Extract parameters
	for (TFieldIterator<FProperty> ParamIt(Function); ParamIt; ++ParamIt)
	{
		FProperty* Param = *ParamIt;

		if (!Param->HasAnyPropertyFlags(CPF_Parm))
		{
			continue;
		}

		FAgentPropertyInfo ParamInfo;
		ParamInfo.PropertyName = Param->GetName();
		ParamInfo.DisplayName = FTypeDiscovery::GetDisplayPropertyName(Param);
		ParamInfo.Type = FPropertyAccessor::GetPropertyType(Param);
		ParamInfo.TypeName = FPropertyAccessor::GetPropertyTypeName(Param);
		ParamInfo.bIsReadOnly = Param->HasAnyPropertyFlags(CPF_ConstParm);

		if (Param->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			Sig.ReturnValue = ParamInfo;
		}
		else if (Param->HasAnyPropertyFlags(CPF_OutParm))
		{
			// Out params are in both Parameters and marked
			ParamInfo.bIsReadOnly = false; // Out params are writable
			Sig.Parameters.Add(ParamInfo);
		}
		else
		{
			Sig.Parameters.Add(ParamInfo);
		}
	}

	return Sig;
}

//~==============================================================================
// Hidden Parameter Detection
//~==============================================================================

bool FFunctionInvoker::FunctionNeedsWorldContext(UFunction* Function)
{
	if (!Function)
	{
		return false;
	}

	// Check for WorldContext metadata
	FString WorldContextMeta = Function->GetMetaData(TEXT("WorldContext"));
	return !WorldContextMeta.IsEmpty();
}

FString FFunctionInvoker::GetWorldContextParamName(UFunction* Function)
{
	if (!Function)
	{
		return TEXT("");
	}

	return Function->GetMetaData(TEXT("WorldContext"));
}

bool FFunctionInvoker::FunctionHasHiddenSelfPin(UFunction* Function)
{
	if (!Function)
	{
		return false;
	}

	// Check for DefaultToSelf metadata
	FString DefaultToSelfMeta = Function->GetMetaData(TEXT("DefaultToSelf"));
	return !DefaultToSelfMeta.IsEmpty();
}

//~==============================================================================
// Function Invocation
//~==============================================================================

FAgentFunctionResult FFunctionInvoker::InvokeFunction(
	UObject* Target,
	UFunction* Function,
	const TMap<FString, FAgentPropertyValue>& Params,
	UWorld* WorldContext)
{
	FAgentFunctionResult Result;

	// Validate inputs
	if (!Function)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Function is null");
		return Result;
	}

	if (!Target && !Function->HasAnyFunctionFlags(FUNC_Static))
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Target is null for non-static function");
		return Result;
	}

	// Derive world context from target if not provided
	if (!WorldContext && Target)
	{
		WorldContext = Target->GetWorld();
	}

	// Prepare parameters
	void* ParamBuffer = PrepareParameters(Function, Params, WorldContext ? WorldContext : Target);
	if (!ParamBuffer && Function->ParmsSize > 0)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Failed to prepare parameters");
		return Result;
	}

	// Invoke the function
	// ProcessEvent handles everything including return values
	if (Target)
	{
		Target->ProcessEvent(Function, ParamBuffer);
	}
	else if (UClass* FuncClass = Function->GetOwnerClass())
	{
		// For truly static functions, use the CDO
		UObject* CDO = FuncClass->GetDefaultObject();
		if (CDO)
		{
			CDO->ProcessEvent(Function, ParamBuffer);
		}
	}

	// Extract results
	Result = ExtractResults(Function, ParamBuffer);
	Result.bSuccess = true;

	// Cleanup
	CleanupParameters(Function, ParamBuffer);

	return Result;
}

FAgentFunctionResult FFunctionInvoker::InvokeStaticFunction(
	UClass* Class,
	UFunction* Function,
	const TMap<FString, FAgentPropertyValue>& Params,
	UWorld* WorldContext)
{
	FAgentFunctionResult Result;

	if (!Class)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Class is null");
		return Result;
	}

	if (!Function)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Function is null");
		return Result;
	}

	// Use the CDO as the target
	UObject* CDO = Class->GetDefaultObject();
	if (!CDO)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Could not get Class Default Object");
		return Result;
	}

	return InvokeFunction(CDO, Function, Params, WorldContext);
}

//~==============================================================================
// Internal Helpers
//~==============================================================================

void* FFunctionInvoker::PrepareParameters(
	UFunction* Function,
	const TMap<FString, FAgentPropertyValue>& Params,
	UObject* WorldContextObject)
{
	if (!Function)
	{
		return nullptr;
	}

	// Allocate parameter buffer
	int32 ParamsSize = Function->ParmsSize;
	if (ParamsSize == 0)
	{
		return nullptr;
	}

	// Allocate and zero the memory
	uint8* ParamBuffer = static_cast<uint8*>(FMemory::Malloc(ParamsSize, Function->GetMinAlignment()));
	FMemory::Memzero(ParamBuffer, ParamsSize);

	// Initialize properties with defaults and provided values
	FString WorldContextParamName = GetWorldContextParamName(Function);

	for (TFieldIterator<FProperty> ParamIt(Function); ParamIt; ++ParamIt)
	{
		FProperty* Param = *ParamIt;

		if (!Param->HasAnyPropertyFlags(CPF_Parm))
		{
			continue;
		}

		// Initialize the property (handles complex types like strings)
		void* ParamPtr = Param->ContainerPtrToValuePtr<void>(ParamBuffer);
		Param->InitializeValue(ParamPtr);

		// Handle WorldContext parameter
		if (!WorldContextParamName.IsEmpty() &&
			Param->GetName() == WorldContextParamName &&
			WorldContextObject)
		{
			if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Param))
			{
				ObjProp->SetObjectPropertyValue(ParamPtr, WorldContextObject);
			}
			continue;
		}

		// Skip return and pure out parameters during setup
		if (Param->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			continue;
		}

		// Check if user provided this parameter
		FString DisplayName = FTypeDiscovery::GetDisplayPropertyName(Param);
		const FAgentPropertyValue* ProvidedValue = Params.Find(DisplayName);

		// Also try the raw name
		if (!ProvidedValue)
		{
			ProvidedValue = Params.Find(Param->GetName());
		}

		if (ProvidedValue)
		{
			// Write the provided value
			FPropertyAccessor::WriteProperty(ParamBuffer, Param, *ProvidedValue);
		}
		// else: keep the default-initialized value
	}

	return ParamBuffer;
}

FAgentFunctionResult FFunctionInvoker::ExtractResults(
	UFunction* Function,
	void* ParamBuffer)
{
	FAgentFunctionResult Result;

	if (!Function || !ParamBuffer)
	{
		return Result;
	}

	for (TFieldIterator<FProperty> ParamIt(Function); ParamIt; ++ParamIt)
	{
		FProperty* Param = *ParamIt;

		if (!Param->HasAnyPropertyFlags(CPF_Parm))
		{
			continue;
		}

		// Extract return value
		if (Param->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			Result.ReturnValue = FPropertyAccessor::ReadProperty(ParamBuffer, Param);
			continue;
		}

		// Extract out parameters
		if (Param->HasAnyPropertyFlags(CPF_OutParm))
		{
			FString DisplayName = FTypeDiscovery::GetDisplayPropertyName(Param);
			FAgentPropertyValue OutValue = FPropertyAccessor::ReadProperty(ParamBuffer, Param);
			Result.OutParams.Add(DisplayName, MakeShared<FAgentPropertyValue>(MoveTemp(OutValue)));
		}
	}

	return Result;
}

void FFunctionInvoker::CleanupParameters(UFunction* Function, void* ParamBuffer)
{
	if (!Function || !ParamBuffer)
	{
		return;
	}

	// Destroy all properties (handles complex types)
	for (TFieldIterator<FProperty> ParamIt(Function); ParamIt; ++ParamIt)
	{
		FProperty* Param = *ParamIt;

		if (!Param->HasAnyPropertyFlags(CPF_Parm))
		{
			continue;
		}

		void* ParamPtr = Param->ContainerPtrToValuePtr<void>(ParamBuffer);
		Param->DestroyValue(ParamPtr);
	}

	// Free the buffer
	FMemory::Free(ParamBuffer);
}

FProperty* FFunctionInvoker::FindParameterByName(UFunction* Function, const FString& ParamName)
{
	if (!Function || ParamName.IsEmpty())
	{
		return nullptr;
	}

	for (TFieldIterator<FProperty> ParamIt(Function); ParamIt; ++ParamIt)
	{
		FProperty* Param = *ParamIt;

		if (!Param->HasAnyPropertyFlags(CPF_Parm))
		{
			continue;
		}

		// Check display name
		if (FTypeDiscovery::GetDisplayPropertyName(Param).Equals(ParamName, ESearchCase::IgnoreCase))
		{
			return Param;
		}

		// Check raw name
		if (Param->GetName().Equals(ParamName, ESearchCase::IgnoreCase))
		{
			return Param;
		}
	}

	return nullptr;
}
