#include "Core.h"
#include "UnrealClasses.h"
#include "UnPackage.h"
#include "UnTypeinfo.h"

//!! NOTE: UE3 has comments for properties, stored in package.UMetaData'MetaData'

#define HOMEPAGE		"http://www.gildor.org/"


/*-----------------------------------------------------------------------------
	Table of known Unreal classes
-----------------------------------------------------------------------------*/

static void RegisterUnrealClasses()
{
	// classes and structures
BEGIN_CLASS_TABLE
	REGISTER_TYPEINFO_CLASSES
#if UNREAL3
	REGISTER_TYPEINFO_CLASSES_U3
#endif
#if MKVSDC
	REGISTER_TYPEINFO_CLASSES_MK
#endif
END_CLASS_TABLE
}


/*-----------------------------------------------------------------------------
	Display class
-----------------------------------------------------------------------------*/

bool GDumpCPP = false;

void DumpProps(FArchive &Ar, const UStruct *Struct);
void DumpHelperTypesForCPP(FArchive &Ar, const UStruct *Struct);

static const char indentBase[] = "\t\t\t\t\t\t\t\t\t\t\t";
static const char *fieldIndent = indentBase + ARRAY_COUNT(indentBase) - 1;

static void Dump(FArchive &Ar, const UFunction *Func)
{
	Ar.Printf("%sfunction %s();", fieldIndent, Func->Name);
}

static void Dump(FArchive &Ar, const UEnum *Enum)
{
	Ar.Printf("%senum %s\n%s{\n", fieldIndent, Enum->Name, fieldIndent);
	for (int i = 0; i < Enum->Names.Num(); i++)
		Ar.Printf("%s\t%s,\n", fieldIndent, *Enum->Names[i]);
	Ar.Printf("%s};\n", fieldIndent);
}

static void Dump(FArchive &Ar, const UConst *Const)
{
	Ar.Printf(GDumpCPP ? "%sstatic constexpr auto %s = %s;" : "%sconst %s = %s;", fieldIndent, Const->Name, *Const->Value);
}

static void Dump(FArchive &Ar, const UStruct *Struct)
{
	Ar.Printf("%sstruct %s\n%s{", fieldIndent, GDumpCPP ? va("F%s", Struct->Name) : Struct->Name, fieldIndent);
	fieldIndent--;
	if (GDumpCPP)
	{
		DumpHelperTypesForCPP(Ar, Struct);
	}
	DumpProps(Ar, Struct);
	fieldIndent++;
	Ar.Printf("\n%s};\n", fieldIndent);
}

static void DumpProperty(FArchive &Ar, const UProperty *Prop, const char *Type, const char *StructName)
{
	if (!GDumpCPP)
	{
		Ar.Printf("%svar", fieldIndent);
		if (Prop->PropertyFlags & 1)
		{
			Ar.Printf("(");
			if (strcmp(*Prop->Category, "None") != 0 && strcmp(*Prop->Category, StructName) != 0)
				Ar.Printf("%s", *Prop->Category);
			Ar.Printf(")");
		}

#define FLAG(v,name)	if (Prop->PropertyFlags & v) Ar.Printf(" "#name);
		FLAG(0x0001000, native    )
		FLAG(0x0000002, const     )
		FLAG(0x0020000, editconst )
		FLAG(0x4000000, editinline)
		FLAG(0x0002000, transient )		//?? sometimes may be "private"
		FLAG(0x0800000, noexport  )
		FLAG(0x0004000, config    )
#undef FLAG
	}
	else
	{
		Ar.Printf("%s", fieldIndent);
#define FLAG(v,name)	if (Prop->PropertyFlags & v) Ar.Printf(#name" ");
		FLAG(0x0000002, const     )
#undef FLAG
	}
	Ar.Printf("%s%s %s", GDumpCPP ? "" : " ", Type, Prop->Name);
	if (Prop->ArrayDim > 1)
		Ar.Printf("[%d]", Prop->ArrayDim);
	Ar.Printf(";");
	if (Prop->PropertyFlags || Prop->PropertyFlags2)
	{
		Ar.Printf("\t// ");
		if (Prop->PropertyFlags2)	// UE3's int64
			Ar.Printf("%08X:", Prop->PropertyFlags2);
		Ar.Printf("%08X", Prop->PropertyFlags);
	}
}

void DumpHelperTypesForCPP(FArchive &Ar, const UStruct *Struct)
{
	guard(DumpHelperTypesForCPP);
	
	assert(GDumpCPP);

	const UField *Next = NULL;
	for (const UField *F = Struct->Children; F; F = Next)
	{
		Next = F->Next;
		const char *ClassName = F->GetClassName();
#define IS(Type)		(strcmp(ClassName, #Type+1)==0)
#define CVT(Type)		const Type *Prop = static_cast<const Type*>(F);
#define DUMP(Type)					\
			if (IS(Type))			\
			{						\
				CVT(Type);			\
				Ar.Printf("\n");	\
				Dump(Ar, Prop);		\
				continue;			\
			}

		// Actually, don't care about functions.
		//DUMP(UFunction)
		if (IS(UFunction)) continue;
		else DUMP(UEnum)
#if UNREAL3
		else DUMP(UScriptStruct)	// as UStruct
#endif
		else DUMP(UStruct)
		else continue;
#undef IS
#undef CVT
#undef DUMP
	}

	unguardf("Struct=%s", Struct->Name);
}

void DumpProps(FArchive &Ar, const UStruct *Struct)
{
	guard(DumpProps);

	const UField *Next = NULL;
	for (const UField *F = Struct->Children; F; F = Next)
	{
//		appPrintf("field: %s (%s)\n", F->Name, F->GetClassName());
		Next = F->Next;
		const char *ClassName = F->GetClassName();
#define IS(Type)		(strcmp(ClassName, #Type+1)==0)
#define CVT(Type)		const Type *Prop = static_cast<const Type*>(F);
#define DUMP(Type)					\
			if (IS(Type))			\
			{						\
				CVT(Type);			\
				Ar.Printf("\n");	\
				Dump(Ar, Prop);		\
				continue;			\
			}

		// special types; they are dumped outside for C++
		if (GDumpCPP)
		{
			if (IS(UFunction) || IS(UEnum) || IS(UStruct))
			{
				continue;
			}
		}
		else
		{
			DUMP(UFunction);
			DUMP(UEnum);
			DUMP(UStruct);
#if UNREAL3
			DUMP(UScriptStruct);	// as UStruct
#endif
		}
		DUMP(UConst);

		// properties
		if (!F->IsA("Property"))
		{
			appNotify("DumpProps for %s'%s' (%s): object is not a Property", F->GetClassName(), F->Name, Struct->Name);
			continue;
		}

		bool isArray = false;
		if (IS(UArrayProperty))
		{
			const UArrayProperty *Arr = static_cast<const UArrayProperty*>(F);
			F  = Arr->Inner;
			if (!F)
			{
				appNotify("ArrayProperty %s.%s has no inner field", Struct->Name, Arr->Name);
				Ar.Printf("\n");
				DumpProperty(Ar, Arr, GDumpCPP ? "TArray<unknown>" : "array<unknown>", Struct->Name);
				continue;
			}
			else
				ClassName = F->GetClassName();
			isArray = true;
		}
		const char *TypeName = NULL;
		if (IS(UByteProperty))
		{
			CVT(UByteProperty);
			TypeName = (Prop->Enum) ? Prop->Enum->Name : "byte";		//?? #IMPORTS#
		}
		else if (IS(UMapProperty))
		{
			TypeName = GDumpCPP ? "TMap<>" : "map<>";		//!! implement
		}
		else if (IS(UIntProperty))
			TypeName = "int";
		else if (IS(UBoolProperty))
			TypeName = "bool";
		else if (IS(UFloatProperty))
			TypeName = "float";
		else if (IS(UObjectProperty))
		{
			CVT(UObjectProperty);
			if (GDumpCPP)
			{
				FName HashedTypeName;
				HashedTypeName = (Prop->PropertyClass) ? va("%s%s*", Prop->PropertyClass->IsA("Actor") ? "A" : "U", Prop->PropertyClass->Name) : "UObject*";	//?? #IMPORTS#
				TypeName = *HashedTypeName;
			}
			else
			{
				TypeName = (Prop->PropertyClass) ? Prop->PropertyClass->Name : "object";	//?? #IMPORTS#
			}
		}
		else if (IS(UClassProperty))
			TypeName = GDumpCPP ? "UClass*" : "class";
		else if (IS(UNameProperty))
			TypeName = GDumpCPP ? "FName" : "name";
		else if (IS(UStrProperty))
			TypeName = GDumpCPP ? "FString" : "string";
		else if (IS(UPointerProperty))
			TypeName = GDumpCPP ? "void*" : "pointer";
		else if (IS(UArrayProperty))
			appError("nested arrays for field %s", F->Name);
		else if (IS(UStructProperty))
		{
			CVT(UStructProperty);
			if (GDumpCPP)
			{
				// If there is no struct, we are in trouble, as structs are inlined.
				FName HashedTypeName;
				HashedTypeName = (Prop->Struct) ? va("F%s", Prop->Struct->Name) : "void";	//?? #IMPORTS#
				TypeName = *HashedTypeName;
			}
			else
			{
				TypeName = (Prop->Struct) ? Prop->Struct->Name : "struct";	//?? #IMPORTS#
			}
		}
		else if (IS(UComponentProperty))
		{
			TypeName = GDumpCPP ? (Ar.Engine() >= GAME_UE4_BASE ? "UActorComponent*" : "UComponent*") : "component";	//???
		}
		else if (IS(UInterfaceProperty))
		{
			TypeName = GDumpCPP ? (Ar.Engine() >= GAME_UE4_BASE ? "UInterface*" : "UObject*") : "interface";	//???
		}
#if MKVSDC
		else if (IS(UNativeTypeProperty))
		{
			CVT(UNativeTypeProperty);
			TypeName = Prop->TypeName;
		}
#endif // MKVSDC
#if UNREAL3
		else if (IS(UDelegateProperty))
		{
			TypeName = "delegate";	//???
		}
		else if (IS(UInterfaceProperty))
		{
			TypeName = "interface";	//???
		}
#endif
		if (!TypeName) appError("Unknown type %s for field %s", ClassName, F->Name);
		char FullType[256];
		if (isArray)
			appSprintf(ARRAY_ARG(FullType), GDumpCPP ? "TArray<%s>" : "array<%s>", TypeName);
		else
			appStrncpyz(FullType, TypeName, ARRAY_COUNT(FullType));
		CVT(UProperty);
		Ar.Printf("\n");
		DumpProperty(Ar, Prop, FullType, Struct->Name);
#undef IS
#undef CVT
#undef DUMP
	}

	unguardf("Struct=%s", Struct->Name);
}

void DumpPropTableForCPP(FArchive& Ar, const UStruct* Struct)
{
	guard(DumpPropTableForCPP);

	assert(GDumpCPP);

	const UField *Next = NULL;
	for (const UField *F = Struct->Children; F; F = Next)
	{
		Next = F->Next;
		const char *ClassName = F->GetClassName();
#define IS(Type)		(strcmp(ClassName, #Type+1)==0)
#define CVT(Type)		const Type *Prop = static_cast<const Type*>(F);
#define DUMP(Type, MacroFmt)										\
			if (IS(Type))											\
			{														\
				Ar.Printf("\n%s" MacroFmt, fieldIndent, F->Name);	\
				continue;											\
			}

		// We don't care about non-props.
		if (!F->IsA("Property"))
		{
			continue;
		}

		if (IS(UArrayProperty))
		{
			const UArrayProperty *Arr = static_cast<const UArrayProperty*>(F);
			F  = Arr->Inner;
			if (!F)
			{
				appNotify("ArrayProperty %s.%s has no inner field", Struct->Name, Arr->Name);
				Ar.Printf("\n%sPROP_DROP(%s)\t// TODO", fieldIndent, Arr->Name);
				continue;
			}
			else
			{
				Ar.Printf("\n%sPROP_DROP(%s)\t// TODO", fieldIndent, Arr->Name);	// TODO
				continue;
			}
		}
			 DUMP(UByteProperty, "PROP_ENUM(%s)")
		else DUMP(UMapProperty, "PROP_DROP(%s)\t// TODO")	// TODO
		else DUMP(UIntProperty, "PROP_INT(%s)")
		else DUMP(UBoolProperty, "PROP_BOOL(%s)")
		else DUMP(UFloatProperty, "PROP_FLOAT(%s)")
		else DUMP(UObjectProperty, "PROP_OBJ(%s)")
		else DUMP(UClassProperty, "PROP_OBJ(%s)")
		else DUMP(UNameProperty, "PROP_NAME(%s)")
		else DUMP(UStrProperty, "PROP_ARRAY(%s, char)")
		else DUMP(UPointerProperty, "PROP_DROP(%s)\t// TODO")	// TODO
		else DUMP(UComponentProperty, "PROP_OBJ(%s)")
		else DUMP(UInterfaceProperty, "PROP_OBJ(%s)")
		else if (IS(UArrayProperty))
			appError("nested arrays for field %s", F->Name);
		else if (IS(UStructProperty))
		{
			CVT(UStructProperty);
			// If there is no struct, we are in trouble, as structs are inlined.
			FName HashedTypeName;
			HashedTypeName = (Prop->Struct) ? va("F%s", Prop->Struct->Name) : "void";	//?? #IMPORTS#
			if (0 == stricmp(Prop->Struct->Name, "Vector"))
			{
				Ar.Printf("\n%sPROP_VECTOR(%s)", fieldIndent, Prop->Name);
			}
			else if (0 == stricmp(Prop->Struct->Name, "Rotator"))
			{
				Ar.Printf("\n%sPROP_ROTATOR(%s)", fieldIndent, Prop->Name);
			}
			else if (0 == stricmp(Prop->Struct->Name, "Color"))
			{
				Ar.Printf("\n%sPROP_COLOR(%s)", fieldIndent, Prop->Name);
			}
			else
			{
				Ar.Printf("\n%sPROP_STRUC(%s, %s)", fieldIndent, Prop->Name, *HashedTypeName);
			}
		}
#if MKVSDC
		else if (IS(UNativeTypeProperty))
		{
			CVT(UNativeTypeProperty);
			Ar.Printf("\n%s_PROP_BASE(%s, %s)", fieldIndent, Prop->Name, Prop->TypeName);
		}
#endif // MKVSDC
#if UNREAL3
		else if (IS(UDelegateProperty))
		{
			Ar.Printf("\n%sPROP_DROP(%s)\t// TODO", fieldIndent, F->Name, "delegate");	//???
		}
#endif
		else appError("Unknown type %s for field %s", ClassName, F->Name);
#undef IS
#undef CVT
#undef DUMP
	}

	unguardf("Struct=%s", Struct->Name);
}

static void DumpDefaultsRecursive(FArchive& Ar, const CTypeInfo* TypeInfo, byte* ThisDefaults, size_t ThisDefaultsSize, byte* SuperDefaults, size_t SuperDefaultsSize)
{
	guard(DumpDefaultsRecursive);

	if (TypeInfo->Parent)
	{
		DumpDefaultsRecursive(Ar, TypeInfo->Parent, ThisDefaults, ThisDefaultsSize, SuperDefaults, SuperDefaultsSize);
	}

	/*for (int i = 0; i < TypeInfo->NumProps; ++i)
	{
		const CPropInfo& Prop = TypeInfo->Props[i];

		bool bDifferentFromSuper = true;
		if (SuperDefaults)
		{
			bDifferentFromSuper = 0 != memcmp(ThisDefaults + Prop.Offset, SuperDefaults + Prop.Offset, Prop.Count * TypeInfo->DumpProps)
		}
	}*/

	unguardf("TypeInfo=%s", TypeInfo->Name);
}

void DumpDefaults(FArchive& Ar, const UStruct* Struct)
{
	guard(DumpDefaults);

	const UStruct* SuperStruct = (UStruct*)Struct->SuperField;

	size_t ThisDefaultsSize = 0, SuperDefaultsSize = 0;
	const byte* ThisDefaults = Struct->GetDefaults(ThisDefaultsSize);
	const byte* SuperDefaults = SuperStruct ? SuperStruct->GetDefaults(SuperDefaultsSize) : nullptr;
	const CTypeInfo* ThisTypeInfo = Struct->GetDataTypeInfo();
	if (ThisDefaults && ThisDefaultsSize > 0)
	{
		assert(!SuperDefaults || SuperDefaultsSize <= ThisDefaultsSize);

		const CTypeInfo* SuperTypeInfo = SuperStruct ? SuperStruct->GetDataTypeInfo() : nullptr;

		//if (ThisTypeInfo->)

		const UField *Next = NULL;
		for (const UField *F = Struct->Children; F; F = Next)
		{
			Next = F->Next;
			const char *ClassName = F->GetClassName();
#define IS(Type)		(strcmp(ClassName, #Type+1)==0)
#define CVT(Type)		const Type *Prop = static_cast<const Type*>(F);
#define DUMP(Type)										\
			if (IS(Type))											\
			{														\
				Ar.Printf("\n%s", fieldIndent, F->Name);	\
				continue;											\
			}

			// We don't care about non-props.
			if (!F->IsA("Property"))
			{
				continue;
			}
			else if (IS(UByteProperty))
			{

			}
#undef IS
#undef CVT
#undef DUMP
		}
	}

	ThisTypeInfo->DumpProps(Ar, ThisDefaults, SuperDefaults, SuperDefaults + SuperDefaultsSize);

	unguardf("Struct=%s", Struct->Name);
}

void DumpClass(const UClass *Class)
{
	//!! NOTE: UProperty going in correct format, other UField data in reverse format
	char Filename[256];
	appSprintf(ARRAY_ARG(Filename), "%s/%s.%s", Class->Package->Name, Class->Name, GDumpCPP ? "h" : "uc");
	FFileWriter Ar(Filename);
	if (GDumpCPP)
	{
		DumpHelperTypesForCPP(Ar, Class);
		Ar.Printf("\n");
	}
	Ar.Printf("class %s%s", GDumpCPP ? (Class->IsA("Actor") ? "A" : "U") : "", Class->Name);
	//?? note: import may be failed when placed in a different package - so, SuperField may be NULL
	//?? when parsing Engine class, derived from Core.Object
	//?? other places with same issue marked as "#IMPORTS#"
	if (Class->SuperField)
		Ar.Printf(" %s %s%s", GDumpCPP ? ": public" : "extends", GDumpCPP ? (Class->SuperField->IsA("Actor") ? "A" : "U") : "", Class->SuperField->Name);
	Ar.Printf(GDumpCPP ? "\n{\n" : ";\n");
	if (GDumpCPP)
	{
		--fieldIndent;
		const char* ThisNativeName = appStrdup(va("%c%s", Class->IsA("Actor") ? 'A' : 'U', Class->Name));
		const char* SuperNativeName = appStrdup(va("%c%s", (Class->SuperField && Class->SuperField->IsA("Actor")) ? 'A' : 'U', Class->SuperField ? Class->SuperField->Name : "Object"));
		Ar.Printf("%sDECLARE_CLASS(%s, %s);\npublic:\n"
			"%s%s::%s()\n"
				"%s\t: %s()\n"
			"%s{",
			fieldIndent, ThisNativeName, SuperNativeName,
			fieldIndent, ThisNativeName, ThisNativeName,
			fieldIndent, SuperNativeName,
			fieldIndent);
		--fieldIndent;
		DumpDefaults(Ar, Class);
		++fieldIndent;
		Ar.Printf("\n%s}\n", fieldIndent);
	}
	DumpProps(Ar, Class);
	if (GDumpCPP)
	{
		Ar.Printf("\n\n%sBEGIN_PROP_TABLE", fieldIndent);
		--fieldIndent;
		DumpPropTableForCPP(Ar, Class);
		++fieldIndent;
		Ar.Printf("\n%sEND_PROP_TABLE", fieldIndent);
		++fieldIndent;
	}
	Ar.Printf(GDumpCPP ? "\n};\n" : "\n");
	Ar.Printf("defaultproperties\n{");
	DumpDefaults(Ar, Class);
	Ar.Printf("}\n");
}

bool DumpTextBuffer(const UTextBuffer *Text)
{
	if (!Text->Text.Len()) return false;		// empty

	// get class name (UTextBuffer's outer UPackage)
	const FObjectExport &Exp = Text->Package->GetExport(Text->PackageIndex);
	const char *ClassName = Text->Package->GetExport(Exp.PackageIndex-1).ObjectName;

	char Filename[256];
	appSprintf(ARRAY_ARG(Filename), "%s/%s.uc", Text->Package->Name, ClassName);
	FFileWriter Ar(Filename);
	Ar.Serialize((void*)*Text->Text, Text->Text.Len());

	return true;
}


/*-----------------------------------------------------------------------------
	Main function
-----------------------------------------------------------------------------*/

int main(int argc, char **argv)
{
#if DO_GUARD
	TRY {
#endif

	guard(Main);

	// display usage
	if (argc < 2)
	{
	help:
		appPrintf("Unreal typeinfo dumper\n"
				  "Usage: typeinfo [options] <package filename>\n"
				  "\n"
				  "Options:\n"
				  "    -text              use TextBuffer object instead of decompilation\n"
				  "    -lzo|lzx|zlib      force compression method for fully-compressed packages\n"
				  "    -cpp               dump using C++ instead of UnrealScript\n"
				  "\n"
				  "For details and updates please visit " HOMEPAGE "\n"
		);
		exit(0);
	}

	// parse command line
	int arg = 1;
	bool UseTextBuffer = false;
	for (arg = 1; arg < argc; arg++)
	{
		if (argv[arg][0] == '-')
		{
			const char *opt = argv[arg]+1;
			if (!stricmp(opt, "text"))
				UseTextBuffer = true;
			else if (!stricmp(opt, "lzo"))
				GForceCompMethod = COMPRESS_LZO;
			else if (!stricmp(opt, "zlib"))
				GForceCompMethod = COMPRESS_ZLIB;
			else if (!stricmp(opt, "lzx"))
				GForceCompMethod = COMPRESS_LZX;
			else if (!stricmp(opt, "cpp"))
				GDumpCPP = true;
			else
				goto help;
		}
		else
		{
			break;
		}
	}
	const char *argPkgName = argv[arg];
	if (!argPkgName) goto help;
	RegisterUnrealClasses();

	// setup NotifyInfo to describe package only
	appSetNotifyHeader(argPkgName);
	// setup root directory
	if (strchr(argPkgName, '/') || strchr(argPkgName, '\\'))
		appSetRootDirectory2(argPkgName);
	else
		appSetRootDirectory(".");
	// load a package
	UnPackage *Package = UnPackage::LoadPackage(argPkgName);
	if (!Package)
	{
		appPrintf("ERROR: Unable to find/load package %s\n", argPkgName);
		exit(1);
	}

	guard(ProcessPackage);
	// extract package name, create directory for it
	char PkgName[256];
	const char *s = strrchr(argPkgName, '/');
	if (!s) s = strrchr(argPkgName, '\\');			// WARNING: not processing mixed '/' and '\'
	if (s) s++; else s = argPkgName;
	appStrncpyz(PkgName, s, ARRAY_COUNT(PkgName));
	char *s2 = strchr(PkgName, '.');
	if (s2) *s2 = 0;
	appMakeDirectory(PkgName);

	if (UseTextBuffer)
	{
		bool dumped = true;
		for (int idx = 0; idx < Package->Summary.ExportCount; idx++)
		{
			const FObjectExport &Exp = Package->GetExport(idx);
			if (stricmp(Package->GetObjectName(Exp.ClassIndex), "TextBuffer") != 0)
				continue;		// not UTextBuffer
			if (stricmp(Exp.ObjectName, "ScriptText"))
				continue;
			UObject *Obj = Package->CreateExport(idx);
			assert(Obj);
			assert(Obj->IsA("TextBuffer"));
			if (!DumpTextBuffer(static_cast<UTextBuffer*>(Obj)))
			{
				dumped = false;
				appPrintf("Error: empty TextBuffer, switching to decompiler\n");
				break;
			}
		}
		if (dumped) return 0;
	}

	guard(LoadWholePackage);
	// load whole package
	for (int idx = 0; idx < Package->Summary.ExportCount; idx++)
	{
		const FObjectExport &Exp = Package->GetExport(idx);
		if (strcmp(Package->GetObjectName(Exp.ClassIndex), "Class") != 0)
			continue;		// not a class
		if (!strcmp(Exp.ObjectName, "None"))
			continue;		// Class'None'
		UObject *Obj = Package->CreateExport(idx);
		assert(Obj);
		assert(Obj->IsA("Class"));
		DumpClass(static_cast<UClass*>(Obj));
	}
	unguard;

	unguard;

	unguard;

#if DO_GUARD
	} CATCH {
		if (GErrorHistory[0])
		{
//			appPrintf("ERROR: %s\n", GErrorHistory);
			appNotify("ERROR: %s\n", GErrorHistory);
		}
		else
		{
//			appPrintf("Unknown error\n");
			appNotify("Unknown error\n");
		}
		exit(1);
	}
#endif
	return 0;
}

int UE4UnversionedPackage(int verMin, int verMax)
{
	appError("Unversioned UE4 packages are not supported. Please restart UModel and select UE4 version in range %d-%d using UI or command line.", verMin, verMax);
	return -1;
}
