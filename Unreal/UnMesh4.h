#ifndef __UNMESH4_H__
#define __UNMESH4_H__

#if UNREAL4

#include "UnMesh.h"			// common types
#include "UnMesh3.h"		// animation enums

// forwards
class UMaterialInterface;
class UAnimSequence4;

#define NUM_INFLUENCES_UE4				4
#define MAX_TOTAL_INFLUENCES_UE4		8
#define MAX_SKELETAL_UV_SETS_UE4		4
#define MAX_STATIC_UV_SETS_UE4			8
#define MAX_STATIC_LODS_UE4				4

//!! These classes should be explicitly defined
typedef UObject UStaticMeshSocket;


/*-----------------------------------------------------------------------------
	USkeletalMesh
-----------------------------------------------------------------------------*/

struct FSkeletalMaterial;
struct FStaticLODModel4;
struct FSkeletalMeshLODInfo;

struct FMeshBoneInfo
{
	FName					Name;
	int						ParentIndex;

	friend FArchive& operator<<(FArchive& Ar, FMeshBoneInfo& B);
};

struct FReferenceSkeleton
{
	TArray<FMeshBoneInfo>	RefBoneInfo;
	TArray<FTransform>		RefBonePose;
	TMap<FName, int>		NameToIndexMap;

	friend FArchive& operator<<(FArchive& Ar, FReferenceSkeleton& S)
	{
		guard(FReferenceSkeleton<<);

		Ar << S.RefBoneInfo;
		Ar << S.RefBonePose;

		if (Ar.ArVer >= VER_UE4_REFERENCE_SKELETON_REFACTOR)
			Ar << S.NameToIndexMap;
		if (Ar.ArVer < VER_UE4_FIXUP_ROOTBONE_PARENT && S.RefBoneInfo.Num() && S.RefBoneInfo[0].ParentIndex != INDEX_NONE)
			S.RefBoneInfo[0].ParentIndex = INDEX_NONE;

		return Ar;

		unguard;
	}
};


struct FReferencePose
{
	FName					PoseName;
	TArray<FTransform>		ReferencePose;

	friend FArchive& operator<<(FArchive& Ar, FReferencePose& P)
	{
		return Ar << P.PoseName << P.ReferencePose;
		//!! TODO: non-cooked package also has P.ReferenceMesh (no way to detect if this is saved or not!)
	}
};

struct FBoneReference
{
	FName					BoneName;

	friend FArchive& operator<<(FArchive& Ar, FBoneReference& B)
	{
		return Ar << B.BoneName;
	}
};

struct FCurveMetaData
{
	bool					bMaterial;
	bool					bMorphtarget;
	TArray<FBoneReference>	LinkedBones;

	friend FArchive& operator<<(FArchive& Ar, FCurveMetaData& D)
	{
		return Ar << D.bMaterial << D.bMorphtarget << D.LinkedBones;
	}
};

struct FSmartNameMapping
{
	//!! TODO: these fields are obsolete
	int16					NextUid;
	TMap<int16, FName>		UidMap;

	friend FArchive& operator<<(FArchive& Ar, FSmartNameMapping& N)
	{
		FFrameworkObjectVersion::Type FrwVer = FFrameworkObjectVersion::Get(Ar);
		if (FrwVer < FFrameworkObjectVersion::SmartNameRefactor)
		{
			return Ar << N.NextUid << N.UidMap;
		}
		// UE4.13+
		TMap<FName, FGuid> GuidMap;
		Ar << GuidMap;
		if (FrwVer >= FFrameworkObjectVersion::MoveCurveTypesToSkeleton)
		{
			// UE4.14+
			TMap<FName, FCurveMetaData> CurveMetaDataMap;
			Ar << CurveMetaDataMap;
		}
		return Ar;
	}
};


struct FSmartNameContainer
{
	TMap<FName, FSmartNameMapping> NameMappings;

	friend FArchive& operator<<(FArchive& Ar, FSmartNameContainer& C)
	{
		return Ar << C.NameMappings;
	}
};


class USkeleton : public UObject
{
	DECLARE_CLASS(USkeleton, UObject);
public:
	FReferenceSkeleton		ReferenceSkeleton;
	TMap<FName, FReferencePose> AnimRetargetSources;
	FGuid					Guid;
	FSmartNameContainer		SmartNames;
	//!! TODO: sockets

	BEGIN_PROP_TABLE
		PROP_DROP(BoneTree)			// was deprecated before 4.0 release in favor of ReferenceSkeleton
		PROP_DROP(Notifies)			// not working with notifies in our program
	END_PROP_TABLE

	USkeleton()
	:	ConvertedAnim(NULL)
	{}

	virtual ~USkeleton();

	// generated stuff
	TArray<UAnimSequence4*>	Anims;
	CAnimSet				*ConvertedAnim;

	virtual void Serialize(FArchive &Ar);
	virtual void PostLoad();

	void ConvertAnims(UAnimSequence4* Seq);
};


class USkeletalMesh4 : public UObject
{
	DECLARE_CLASS(USkeletalMesh4, UObject);
public:
	FBoxSphereBounds		Bounds;
	TArray<FSkeletalMaterial> Materials;
	FReferenceSkeleton		RefSkeleton;
	USkeleton				*Skeleton;
	TArray<FStaticLODModel4> LODModels;
	TArray<FSkeletalMeshLODInfo> LODInfo;

	// properties
	bool					bHasVertexColors;

	CSkeletalMesh			*ConvertedMesh;

	BEGIN_PROP_TABLE
		PROP_OBJ(Skeleton)
		PROP_BOOL(bHasVertexColors)
		PROP_ARRAY(LODInfo, FSkeletalMeshLODInfo)
	END_PROP_TABLE

	USkeletalMesh4();
	virtual ~USkeletalMesh4();

	virtual void Serialize(FArchive &Ar);

protected:
	void ConvertMesh();
};


/*-----------------------------------------------------------------------------
	UDestructibleMesh
-----------------------------------------------------------------------------*/

class UDestructibleMesh : public USkeletalMesh4
{
	DECLARE_CLASS(UDestructibleMesh, USkeletalMesh4);

	virtual void Serialize(FArchive& Ar)
	{
		Super::Serialize(Ar);
		// This class has lots of PhysX data
		DROP_REMAINING_DATA(Ar);
	}
};

/*-----------------------------------------------------------------------------
	UStaticMesh
-----------------------------------------------------------------------------*/

// forwards (structures are declared in cpp)
struct FStaticMeshLODModel4;

struct FMeshBuildSettings
{
	DECLARE_STRUCT(FMeshBuildSettings);

	bool					bRecomputeNormals;
	bool					bRecomputeTangents;

	BEGIN_PROP_TABLE
		PROP_BOOL(bRecomputeNormals)
		PROP_BOOL(bRecomputeTangents)
		PROP_DROP(bUseMikkTSpace)
		PROP_DROP(bRemoveDegenerates)
		PROP_DROP(bBuildAdjacencyBuffer)
		PROP_DROP(bBuildReversedIndexBuffer)
		PROP_DROP(bUseFullPrecisionUVs)
		PROP_DROP(bGenerateLightmapUVs)
		PROP_DROP(MinLightmapResolution)
		PROP_DROP(SrcLightmapIndex)
		PROP_DROP(DstLightmapIndex)
		PROP_DROP(BuildScale3D)						//?? FVector, use?
		PROP_DROP(DistanceFieldResolutionScale)
		PROP_DROP(bGenerateDistanceFieldAsIfTwoSided)
		PROP_DROP(DistanceFieldReplacementMesh)
	END_PROP_TABLE
};

struct FStaticMeshSourceModel
{
	DECLARE_STRUCT(FStaticMeshSourceModel);
	FByteBulkData			BulkData;
	FMeshBuildSettings		BuildSettings;

	BEGIN_PROP_TABLE
		PROP_STRUC(BuildSettings, FMeshBuildSettings)
		PROP_DROP(ReductionSettings)
		PROP_DROP(ScreenSize)
	END_PROP_TABLE
};

class UStaticMesh4 : public UObject
{
	DECLARE_CLASS(UStaticMesh4, UObject);
public:
	CStaticMesh				*ConvertedMesh;

	UObject					*BodySetup;			// UBodySetup
	UObject*				NavCollision;		// UNavCollision
	FGuid					LightingGuid;
	bool					bUseHighPrecisionTangentBasis;

	TArray<UStaticMeshSocket*> Sockets;

	TArray<UMaterialInterface*> Materials;

	// FStaticMeshRenderData fields
	FBoxSphereBounds		Bounds;
	float					ScreenSize[MAX_STATIC_LODS_UE4];
	float					StreamingTextureFactors[MAX_STATIC_UV_SETS_UE4];
	float					MaxStreamingTextureFactor;
	bool					bReducedBySimplygon;
	bool					bLODsShareStaticLighting;
	TArray<FStaticMeshLODModel4> Lods;
	TArray<FStaticMeshSourceModel> SourceModels;

	BEGIN_PROP_TABLE
		PROP_ARRAY(Materials, UObject*)
		PROP_ARRAY(SourceModels, FStaticMeshSourceModel)
	END_PROP_TABLE

	UStaticMesh4();
	virtual ~UStaticMesh4();

	virtual void Serialize(FArchive &Ar);

protected:
	void ConvertMesh();
	void ConvertSourceModels();
};


/*-----------------------------------------------------------------------------
	UAnimSequence
-----------------------------------------------------------------------------*/

//!! References:
//!! CompressAnimationsFunctor() - Source/Editor/UnrealEd/Private/Commandlets/PackageUtilities.cpp

struct FCompressedOffsetData
{
	DECLARE_STRUCT(FCompressedOffsetData);
public:
	TArray<int32>			OffsetData;
	int32					StripSize;

	BEGIN_PROP_TABLE
		PROP_ARRAY_POD(OffsetData, int)
		PROP_INT(StripSize)
	END_PROP_TABLE

	friend FArchive& operator<<(FArchive& Ar, FCompressedOffsetData& D)
	{
		return Ar << D.OffsetData << D.StripSize;
	}
};


struct FTrackToSkeletonMap
{
	DECLARE_STRUCT(FTrackToSkeletonMap);

	int32					BoneTreeIndex;

	BEGIN_PROP_TABLE
		PROP_INT(BoneTreeIndex)
	END_PROP_TABLE

	friend FArchive& operator<<(FArchive& Ar, FTrackToSkeletonMap& M)
	{
		return Ar << M.BoneTreeIndex;
	}
};


struct FRawCurveTracks
{
	DECLARE_STRUCT(FRawCurveTracks);

//	TArray<FFloatCurve>		FloatCurves;

	BEGIN_PROP_TABLE
	#if 1
		PROP_DROP(FloatCurves)	// remove when implement float curves
	#else
		PROP_ARRAY(FloatCurves, FFloatCurve)
	#endif
	END_PROP_TABLE

	friend FArchive& operator<<(FArchive& Ar, FRawCurveTracks& T)
	{
		guard(FRawCurveTracks<<);
		// This structure is always serialized as property list
		FRawCurveTracks::StaticGetTypeinfo()->SerializeProps(Ar, &T);
		return Ar;
		unguard;
	}
};


class UAnimationAsset : public UObject
{
	DECLARE_CLASS(UAnimationAsset, UObject);
public:
	USkeleton*				Skeleton;
	FGuid					SkeletonGuid;

	BEGIN_PROP_TABLE
		PROP_OBJ(Skeleton)
	END_PROP_TABLE

	UAnimationAsset()
	:	Skeleton(NULL)
	{}

	virtual void Serialize(FArchive& Ar)
	{
		Super::Serialize(Ar);
		if (Ar.ArVer >= VER_UE4_SKELETON_GUID_SERIALIZATION)
			Ar << SkeletonGuid;
	}
};


class UAnimSequenceBase : public UAnimationAsset
{
	DECLARE_CLASS(UAnimSequenceBase, UAnimationAsset);
public:
//	virtual void Serialize(FArchive& Ar) - almost empty implementation
};


class UAnimSequence4 : public UAnimSequenceBase
{
	DECLARE_CLASS(UAnimSequence4, UAnimSequenceBase);
public:
	int32					NumFrames;
	float					RateScale;
	float					SequenceLength;
	TArray<FRawAnimSequenceTrack> RawAnimationData;
	TArray<uint8>			CompressedByteStream;
	bool					bUseRawDataOnly;

	AnimationKeyFormat		KeyEncodingFormat;
	AnimationCompressionFormat TranslationCompressionFormat;
	AnimationCompressionFormat RotationCompressionFormat;
	AnimationCompressionFormat ScaleCompressionFormat;
	TArray<int32>			CompressedTrackOffsets;
	FCompressedOffsetData	CompressedScaleOffsets;
	TArray<FTrackToSkeletonMap> TrackToSkeletonMapTable;			// used for raw data
	TArray<FTrackToSkeletonMap> CompressedTrackToSkeletonMapTable;	// used for compressed data, missing before 4.12
	FRawCurveTracks			CompressedCurveData;

	BEGIN_PROP_TABLE
		PROP_INT(NumFrames)
		PROP_FLOAT(RateScale)
		PROP_FLOAT(SequenceLength)
		// Before UE4.12 some fields were serialized as properties
		PROP_ENUM2(KeyEncodingFormat, AnimationKeyFormat)
		PROP_ENUM2(TranslationCompressionFormat, AnimationCompressionFormat)
		PROP_ENUM2(RotationCompressionFormat, AnimationCompressionFormat)
		PROP_ENUM2(ScaleCompressionFormat, AnimationCompressionFormat)
		PROP_ARRAY_POD(CompressedTrackOffsets, int)
		PROP_STRUC(CompressedScaleOffsets, FCompressedOffsetData)
		PROP_ARRAY(TrackToSkeletonMapTable, FTrackToSkeletonMap)
		PROP_ARRAY(CompressedTrackToSkeletonMapTable, FTrackToSkeletonMap)
		PROP_STRUC(CompressedCurveData, FRawCurveTracks)
	END_PROP_TABLE

	UAnimSequence4()
	:	RateScale(1.0f)
	,	TranslationCompressionFormat(ACF_None)
	,	RotationCompressionFormat(ACF_None)
	,	ScaleCompressionFormat(ACF_None)
	,	KeyEncodingFormat(AKF_ConstantKeyLerp)
	{}

	virtual void Serialize(FArchive& Ar);
	virtual void PostLoad();

	int GetNumTracks() const;
	int GetTrackBoneIndex(int TrackIndex) const;
	int FindTrackForBoneIndex(int BoneIndex) const;
};


#define REGISTER_MESH_CLASSES_U4 \
	REGISTER_CLASS(USkeleton) \
	REGISTER_CLASS(FSkeletalMeshLODInfo) \
	REGISTER_CLASS_ALIAS(USkeletalMesh4, USkeletalMesh) \
	REGISTER_CLASS(UDestructibleMesh) \
	REGISTER_CLASS_ALIAS(UStaticMesh4, UStaticMesh) \
	REGISTER_CLASS(FMeshBuildSettings) \
	REGISTER_CLASS(FStaticMeshSourceModel) \
	REGISTER_CLASS(FCompressedOffsetData) \
	REGISTER_CLASS(FTrackToSkeletonMap) \
	REGISTER_CLASS(FRawCurveTracks) \
	REGISTER_CLASS_ALIAS(UAnimSequence4, UAnimSequence)


#endif // UNREAL4
#endif // __UNMESH4_H__
