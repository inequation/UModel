#ifndef __UNOBJECT_H__
#define __UNOBJECT_H__

/*-----------------------------------------------------------------------------
	Simple RTTI
-----------------------------------------------------------------------------*/

//!! can separate typeinfo into different header

// Comparing PropLevel with Super::PropLevel to detect whether we have property
// table in current class or not
#define DECLARE_BASE(IsClass,Class,Base)		\
	public:										\
		typedef Class	ThisClass;				\
		typedef Base	Super;					\
		static void InternalConstructor(void *Mem) \
		{ new(Mem) ThisClass; }					\
		static const CTypeInfo *StaticGetTypeinfo() \
		{										\
			int numProps = 0;					\
			const CPropInfo *props = NULL;		\
			if ((int)PropLevel != (int)Super::PropLevel) \
				props = StaticGetProps(numProps); \
			static const CTypeInfo type(		\
				IsClass,						\
				#Class + 1,						\
				Super::StaticGetTypeinfo(),		\
				sizeof(ThisClass),				\
				props, numProps,				\
				InternalConstructor				\
			);									\
			return &type;						\
		}

#define DECLARE_STRUCT(Class)					\
		DECLARE_BASE(false, Class, CNullType)	\
		const CTypeInfo *GetTypeinfo() const	\
		{										\
			return StaticGetTypeinfo();			\
		}

// structure derived from another structure with typeinfo
#define DECLARE_STRUCT2(Class,Base)				\
		DECLARE_BASE(false, Class, Base)		\
		const CTypeInfo *GetTypeinfo() const	\
		{										\
			return StaticGetTypeinfo();			\
		}

//?? can replace virtual GetClassName() with GetTypeinfo()->Name
#define DECLARE_CLASS(Class,Base)				\
		DECLARE_BASE(true, Class, Base)			\
		virtual const CTypeInfo *GetTypeinfo() const \
		{										\
			return StaticGetTypeinfo();			\
		}										\
	private:									\
		friend FArchive& operator<<(FArchive &Ar, Class *&Res) \
		{										\
			return Ar << *(UObject**)&Res;		\
		}


struct CPropInfo
{
	const char	   *Name;		// field name
	const char	   *TypeName;	// name of the field type
	uint16			Offset;		// offset of this field from the class start
	int16			Count;		// number of array items
	size_t			SizeOf;		// size of the field type in bytes
};


struct CTypeConstructor
{
	typedef void (*FRawFunc)(void* Mem);

	CTypeConstructor(std::nullptr_t)
		: bIsRawFunc(true)
		, Ptr({ nullptr })
	{}

	CTypeConstructor(FRawFunc InRawFunc)
		: bIsRawFunc(true)
	{
		Ptr.RawFunc = InRawFunc;
	}

	CTypeConstructor(class UStruct* InStruct)
		: bIsRawFunc(false)
	{
		Ptr.Struct = InStruct;
	}

	CTypeConstructor(const CTypeConstructor& Other)
		: bIsRawFunc(Other.bIsRawFunc)
		, Ptr(Other.Ptr)
	{}

	void operator()(void* Mem) const;
	operator bool() const { return nullptr != (bIsRawFunc ? (void*)Ptr.RawFunc : (void*)Ptr.Struct); }

private:
	bool			bIsRawFunc;
	union
	{
		FRawFunc		RawFunc;
		class UStruct*	Struct;
	}					Ptr;
};


struct CTypeInfo
{
	bool			bIsClass;	// Is simply a struct if false.
	const char		*Name;
	const CTypeInfo *Parent;
	size_t			SizeOf;
	const CPropInfo *Props;
	int				NumProps;
	CTypeConstructor Constructor;
	// methods
	FORCEINLINE CTypeInfo(const bool bInIsClass, const char *AName, const CTypeInfo *AParent, int DataSize,
					 const CPropInfo *AProps, int PropCount, CTypeConstructor AConstructor)
	:	bIsClass(bInIsClass)
	,	Name(AName)
	,	Parent(AParent)
	,	SizeOf(DataSize)
	,	Props(AProps)
	,	NumProps(PropCount)
	,	Constructor(AConstructor)
	{}
	bool IsA(const char *TypeName) const;
	const CPropInfo *FindProperty(const char *Name) const;
	void SerializeProps(FArchive &Ar, void *ObjectData, const void *ObjectDataEnd = NULL) const;
	void DumpProps(FArchive& Ar, const void *Data, const void *DefaultData = NULL, const void *DefaultDataEnd = NULL) const;
	static void RemapProp(const char *Class, const char *OldName, const char *NewName);
};


// Helper class to simplify DECLARE_CLASS() macro group
// This class is used as Base for DECLARE_BASE()/DECLARE_CLASS() macros
struct CNullType
{
	enum { PropLevel = -1 };		// overriden in BEGIN_CLASS_TABLE
	static FORCEINLINE const CPropInfo *StaticGetProps(int &numProps)
	{
		numProps = 0;
		return NULL;
	}
	static FORCEINLINE const CTypeInfo *StaticGetTypeinfo()
	{
		return NULL;
	}
};


#define _PROP_BASE_EX(Field,Type,TypeStr)	{ #Field, TypeStr, FIELD2OFS(ThisClass, Field), sizeof(((ThisClass*)NULL)->Field) / sizeof(Type), sizeof(((ThisClass*)NULL)->Field) },
#define _PROP_BASE(Field,Type)				_PROP_BASE_EX(Field,Type,#Type)
#define PROP_ARRAY(Field,Type)				{ #Field, #Type, FIELD2OFS(ThisClass, Field), -1, sizeof(((ThisClass*)NULL)->Field) },
#define PROP_STRUC(Field,Type)				_PROP_BASE(Field, Type)
#define PROP_DROP(Field)					{ #Field, NULL, 0, 0, 0 },		// signal property, which should be dropped


// BEGIN_PROP_TABLE/END_PROP_TABLE declares property table inside class declaration
#define BEGIN_PROP_TABLE						\
	enum { PropLevel = Super::PropLevel + 1 };	\
	static FORCEINLINE const CPropInfo *StaticGetProps(int &numProps) \
	{											\
		static const CPropInfo props[] =		\
		{
// simple types
// note: has special PROP_ENUM(), which is the same as PROP_BYTE(), but when using
// PROP_BYTE for enumeration value, _PROP_BASE macro will set 'Count' field of
// CPropInfo to 4 instead of 1 (enumeration values are serialized as byte, but
// compiler report it as 4-byte field)
#define PROP_ENUM(Field)		{ #Field, "byte",   FIELD2OFS(ThisClass, Field), 1, sizeof(((ThisClass*)NULL)->Field) },
#define PROP_ENUM2(Field,Type)	{ #Field, "#"#Type, FIELD2OFS(ThisClass, Field), 1, sizeof(((ThisClass*)NULL)->Field) },
#define PROP_BYTE(Field)		_PROP_BASE(Field, byte     )
#define PROP_INT(Field)			_PROP_BASE(Field, int      )
#define PROP_BOOL(Field)		_PROP_BASE(Field, bool     )
#define PROP_FLOAT(Field)		_PROP_BASE(Field, float    )
#define PROP_NAME(Field)		_PROP_BASE_EX(Field, FName,		"Name"		)
#define PROP_OBJ(Field)			_PROP_BASE_EX(Field, UObject*,	"Object"	)
#define PROP_VECTOR(Field)		_PROP_BASE_EX(Field, FVector,	"Vector"	)
#define PROP_ROTATOR(Field)		_PROP_BASE_EX(Field, FRotator,	"Rotator"	)
#define PROP_COLOR(Field)		_PROP_BASE_EX(Field, FColor,	"Color"		)

#define END_PROP_TABLE							\
		};										\
		numProps = ARRAY_COUNT(props);			\
		return props;							\
	}

#if DECLARE_VIEWER_PROPS
#define VPROP_ARRAY_COUNT(Field,Name)	{ #Name, "int", FIELD2OFS(ThisClass, Field) + ARRAY_COUNT_FIELD_OFFSET, 1 },
#endif // DECLARE_VIEWER_PROPS

// Mix of BEGIN_PROP_TABLE and DECLARE_BASE
// Allows to declare property table outside of class declaration
#define BEGIN_PROP_TABLE_EXTERNAL(Class)		\
static const CTypeInfo* Class##_StaticGetTypeinfo() \
{												\
	static const char ClassName[] = #Class;		\
	typedef Class ThisClass;					\
	static const CPropInfo props[] =			\
	{

// Mix of END_PROP_TABLE and DECLARE_BASE
#define END_PROP_TABLE_EXTERNAL					\
	};											\
	static const CTypeInfo type(				\
		false,									\
		ClassName,								\
		NULL,									\
		sizeof(ThisClass),						\
		props, ARRAY_COUNT(props),				\
		nullptr									\
	);											\
	return &type;								\
}

struct CTypeInfoGetter
{
	typedef const CTypeInfo* (*FRawFunc)();

	CTypeInfoGetter()
		: bIsRawFunc(true)
		, Ptr({ nullptr })
	{}

	CTypeInfoGetter(FRawFunc InRawFunc)
		: bIsRawFunc(true)
	{
		Ptr.RawFunc = InRawFunc;
	}

	CTypeInfoGetter(UStruct* InStruct)
		: bIsRawFunc(false)
	{
		Ptr.Struct = InStruct;
	}

	CTypeInfoGetter(const CTypeInfoGetter& Other)
		: bIsRawFunc(Other.bIsRawFunc)
		, Ptr(Other.Ptr)
	{}

	const CTypeInfo* operator()() const;
	operator bool() const { return nullptr != (bIsRawFunc ? (void*)Ptr.RawFunc : (void*)Ptr.Struct); }

private:
	bool				bIsRawFunc;
	union
	{
		FRawFunc		RawFunc;
		class UStruct*	Struct;
	}					Ptr;
};

enum EClassType
{
	Struct			= 0x01,
	Class			= 0x02,
	Actor			= 0x04 | Class,
	Native			= 0x08,
	Script			= 0x10,

	NativeStruct	= Native | Struct,
	NativeClass		= Native | Class,
	NativeActor		= Native | Actor,
	ScriptStruct	= Script | Struct,
	ScriptClass		= Script | Class,
	ScriptActor		= Script | Actor
};

struct CClassInfo
{
	const char		*Name;
	EClassType		ClassType;
	CTypeInfoGetter	TypeInfo;
};


void RegisterClasses(const CClassInfo *Table, int Count);
void UnregisterClass(const char *Name, bool WholeTree = false);

const CTypeInfo *FindClassType(const char *Name);
const CTypeInfo *FindStructType(const char *Name);

UObject *CreateClass(const char *Name);

FORCEINLINE bool IsKnownClass(const char *Name)
{
	return (FindClassType(Name) != NULL);
}


#define BEGIN_CLASS_TABLE						\
	{											\
		static const CClassInfo Table[] = {
#define REGISTER_NATIVE_TYPE(Type,ClassType)	\
			{ #Type + 1, EClassType::ClassType, Type::StaticGetTypeinfo },
// Native type with BEGIN_PROP_TABLE_EXTERNAL/END_PROP_TABLE_EXTERNAL property table
#define REGISTER_NATIVE_TYPE_EXTERNAL(Type,ClassType)			\
			{ #Type + 1, EClassType::ClassType, Type##_StaticGetTypeinfo },
#define REGISTER_NATIVE_TYPE_ALIAS(Type,TypeName,ClassType)	\
			{ #TypeName + 1, EClassType::ClassType, Type::StaticGetTypeinfo },
#define REGISTER_ACTOR(Class)					\
			REGISTER_NATIVE_TYPE(Class, NativeActor)
// Actor with BEGIN_PROP_TABLE_EXTERNAL/END_PROP_TABLE_EXTERNAL property table
#define REGISTER_ACTOR_EXTERNAL(Class)			\
			REGISTER_NATIVE_TYPE_EXTERNAL(Class, NativeActor)
#define REGISTER_ACTOR_ALIAS(Class,ClassName)	\
			REGISTER_NATIVE_TYPE_ALIAS(Class, ClassName, NativeActor)
#define REGISTER_CLASS(Class)					\
			REGISTER_NATIVE_TYPE(Class, NativeClass)
// Class with BEGIN_PROP_TABLE_EXTERNAL/END_PROP_TABLE_EXTERNAL property table
#define REGISTER_CLASS_EXTERNAL(Class)			\
			REGISTER_NATIVE_TYPE_EXTERNAL(Class, NativeClass)
#define REGISTER_CLASS_ALIAS(Class,ClassName)	\
			REGISTER_NATIVE_TYPE_ALIAS(Class, ClassName, NativeClass)
#define REGISTER_STRUCT(Struct)					\
			REGISTER_NATIVE_TYPE(Struct, NativeStruct)
// Struct with BEGIN_PROP_TABLE_EXTERNAL/END_PROP_TABLE_EXTERNAL property table
#define REGISTER_STRUCT_EXTERNAL(Struct)		\
			REGISTER_NATIVE_TYPE_EXTERNAL(Struct, NativeStruct)
#define REGISTER_STRUCT_ALIAS(Struct,StructName)\
			REGISTER_NATIVE_TYPE_ALIAS(Struct, StructName, NativeStruct)
#define END_CLASS_TABLE							\
		};										\
		RegisterClasses(ARRAY_ARG(Table));		\
	}


struct enumToStr
{
	int			value;
	const char* name;
};

template<class T>
struct TEnumInfo
{
	FORCEINLINE static const char* GetName()
	{
	#ifndef __GNUC__ // this assertion always failed in gcc 4.9
		staticAssert(0, "Working with unregistered enum");
	#endif
		return "UnregisteredEnum";
	}
};

#define _ENUM(name)						\
	template<> struct TEnumInfo<name>	\
	{									\
		FORCEINLINE static const char* GetName() { return #name; } \
	};									\
	const enumToStr name##Names[] =

#define _E(name)				{ name, #name }

#define REGISTER_ENUM(name)		RegisterEnum(#name, ARRAY_ARG(name##Names));

#define ENUM_UNKNOWN			255

void RegisterEnum(const char *EnumName, const enumToStr *Values, int Count);
// find name of enum item, NULL when not found
const char *EnumToName(const char *EnumName, int Value);
// find interer value by enum name, ENUM_UNKNOWN when not found
int NameToEnum(const char *EnumName, const char *Value);

template<class T>
FORCEINLINE const char* EnumToName(T Value)
{
	return EnumToName(TEnumInfo<T>::GetName(), Value);
}

/*-----------------------------------------------------------------------------
	UObject class
-----------------------------------------------------------------------------*/

class UObject
{
	DECLARE_BASE(true, UObject, CNullType)

public:
	// internal storage
	UnPackage		*Package;
	int				PackageIndex;	// index in package export table; INDEX_NONE for non-packaged (transient) object
	const char		*Name;
	UObject			*Outer;
#if UNREAL3
	int				NetIndex;
#endif

//	unsigned	ObjectFlags;

	UObject();
	virtual ~UObject();
	virtual void Serialize(FArchive &Ar);
	virtual void PostLoad()			// called after serializing all objects
	{}

	// RTTI support
	inline bool IsA(const char *ClassName) const
	{
		return GetTypeinfo()->IsA(ClassName);
	}
	inline const char *GetClassName() const
	{
		return GetTypeinfo()->Name;
	}
	const char *GetRealClassName() const;		// class name from the package export table
	const char* GetPackageName() const;
	const char *GetUncookedPackageName() const;
	void GetFullName(char *buf, int bufSize, bool IncludeObjectName = true, bool IncludeCookedPackageName = true, bool ForcePackageName = false) const;

	enum { PropLevel = 0 };
	static FORCEINLINE const CPropInfo *StaticGetProps(int &numProps)
	{
		numProps = 0;
		return NULL;
	}
	virtual const CTypeInfo *GetTypeinfo() const
	{
		return StaticGetTypeinfo();
	}

//private: -- not private to allow object browser ...
	// static data and methods
	static int				GObjBeginLoadCount;
	static TArray<UObject*>	GObjLoaded;
	static TArray<UObject*> GObjObjects;
	static UObject			*GLoadingObj;

	static void BeginLoad();
	static void EndLoad();

	// accessing object's package properties (here just to exclude UnPackage.h whenever possible)
	const FArchive* GetPackageArchive() const;
	int GetGame() const;
	int GetArVer() const;
	int GetLicenseeVer() const;

	void *operator new(size_t Size)
	{
		return appMalloc(Size);
	}
	void *operator new(size_t Size, void *Mem)	// for inplace constructor
	{
		return Mem;
	}
	void operator delete(void *Object)
	{
		appFree(Object);
	}
};


void RegisterCoreClasses();

#endif // __UNOBJECT_H__
