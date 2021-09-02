/*
 *  FbxImporter.h
 *  ionEngine
 *
 *  Created by Claire Rogers on 03/04/2010.
 *  Copyright 2010 Spinning Head Software. All rights reserved.
 *
 */

#pragma once

#define FBXSDK_NEW_API
#include <fbxsdk.h>

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>

static FbxAnimEvaluator *g_pSceneEvaluator;

namespace rengineTools
{
    struct Vertex4Bone
    {
		vec4			srcVertex;
        int				boneIndices[4];
        float           boneWeights[4];
    };
	
	struct Vector4i
	{
		Vector4i();
		Vector4i(int32_t x, int32_t y, int32_t z, int32_t w);
		uint32_t		x, y, z, w;
	};

	struct Vector4f
	{
		Vector4f();
		Vector4f(float x, float y, float z, float w);
		float		x, y, z, w;
	};

	struct Vector2
	{
		float			x, y;
	};	
	
	struct Vertex
	{
		Vertex();
		Vertex( float x, float y, float z );
		float x, y, z;
	};
	
	struct TexCoord
	{
		TexCoord();
		TexCoord( float u, float v );
		float u, v;
	};
	
	struct Normal
	{
		Normal();
		Normal( float x, float y, float z );
		float x, y, z;
	};
	
	struct Color
	{
		Color() {};
		Color(float _r, float _g, float _b) : r(_r), g(_g), b(_b) {}

		float r, g, b;
	};
	
    struct  FbxStaticVertex
    {
        Color color;
        TexCoord uv;
    };
	
	class IonFbxMesh;
	class IonFbxMeshInstance;
	class IonFbxPose;
	class IonFbxTake;
//	class Model;
//	class Mat44;
	class IonFbxTexture;
//	class AABB;
	
	class FbxConverter
	{
	public:
		FbxConverter();
		~FbxConverter();
		
		void ImportFile(std::string filename, std::vector<IonFbxMesh*>& scene);
		void ConvertFile(std::string inputFilename, std::string outputFilename);

	private:
		void ImportFile(std::string filename, std::vector<IonFbxMesh*>& scene, bool bStoreHashes);
		void Convert();
		void ConvertNurbsAndPatchRecursive(FbxManager* pSdkManager, FbxNode* pNode);
		void ExtractPoseAtTime(float time);
		void ExtractPoseAtTimeRecursive(FbxNode* pNode, FbxTime& pTime, FbxMatrix& pParentGlobalPosition, IonFbxMesh* pMesh, int nodeLevel);
		void ExtractPose(FbxNode* pNode, FbxTime& pTime, FbxMatrix& pParentGlobalPosition, FbxMatrix& pGlobalPosition, IonFbxMesh* pMesh);

		void ExtractPoseBones(FbxNode* pNode, FbxTime& pTime, FbxMatrix& pGlobalPosition);
		
		void AnimateSkeleton(float time);
		
		int GetPose(double time);
				
		void Export(std::string filename, std::vector<IonFbxMesh*>&models);
		void ExportLights(FILE* fptr);
		void ExportTextures(FILE *fptr);
		void ExportMaterials(FILE *fptr);
		void ExportSkeleton(FILE *fptr, IonFbxMesh* mesh);
		void ExportAnimation(FILE* fptr);
		void ExportMeshes(FILE *fptr, std::vector<IonFbxMesh*>& models);
		void ExportMeshInstances(FILE *fptr);
		int	 CalcExportMeshSizeRecursive(int currentSize, IonFbxMesh* model);
		uint8_t* ExportMeshRecursive(uint8_t* ptr, IonFbxMesh* model);
//		ion::Import::VertexStream m_stream1ExportTemplate;
//		ion::Import::VertexStream m_stream2ExportTemplate;
		
		FbxManager	*m_pSdkManager;
		FbxScene	*m_pScene;		
		FbxImporter *m_pImporter;

		double		m_frameRate;
		double		m_startTime;
		uint32_t	m_numTakes;
//		uint32_t	m_numFrames;
		uint32_t	m_currentTakeIndex;
		uint32_t	m_currentFrameIndex;
		bool		m_firstFrame;
		IonFbxTake	*m_pTakes;
//		IonFbxPose	*m_pPoses;
		
		uint32_t	m_numMeshes;
        IonFbxMesh*	m_pMeshes;
		bool		m_isSkinned;

		std::unordered_map<FbxMesh*, IonFbxMesh *> m_meshes;
		std::vector<IonFbxMeshInstance *> m_meshInstances;
	public:
	};
	
	class IonFbxPose
	{
	public:
		IonFbxPose();
		~IonFbxPose();
//	private:
		uint32_t	m_numBones;
		mat4x4*		m_bones;							//CLR - Find a smaller more suitable datatype.
	};
	
	class IonFbxTake
	{
	public:
		IonFbxTake() : name(nullptr), startTime(0.0f), endTime(0.0f), numFrames(0), frames(nullptr) {}
		~IonFbxTake() {}

		const char* name;
		float	startTime;
		float	endTime;
		uint32_t numFrames;
		IonFbxPose* frames;
	};

#if 0
	struct FbxFatVert
	{
		FbxFatVert();
		FbxFatVert(Vertex& v, TexCoord& vt);
		Vertex      vertex;
		Vertex		normal;
//		Vertex		tangent;
//		Vertex		binormal;
		TexCoord    texCoord0;
		TexCoord	texCoord1;
		float       weights[4];
		uint32_t    bones[4];
	};
#else	
	struct FbxFatVert
	{
		FbxFatVert()
			: uniqueVertexIndex(0xffffffff)
			, uniqueNormalIndex(0xffffffff)
			, uniqueUvIndices{ 0xffffffff, 0xffffffff }
			, uniqueTangentIndex(0xffffffff)
			, uniqueBinormalIndex(0xffffffff)
		{};

		uint32_t	uniqueVertexIndex;
		uint32_t	uniqueNormalIndex;
		uint32_t	uniqueUvIndices[2];
		uint32_t	uniqueTangentIndex;
		uint32_t	uniqueBinormalIndex;
//		Vector4i	uniqueBoneIndices;
//		Vector4f	uniqueBoneWeights;
	};
#endif

	class VertexBuffer : public reng::Import::VertexBuffer
	{
	public:
		VertexBuffer();
		~VertexBuffer();

		void setNumStreams(uint32_t numStreams);
		void setNumVertices(uint32_t numVertices);

		inline uint32_t getNumStreams()		{ return m_numStreams;	}
		inline uint32_t getNumVertices()	{ return m_numVertices;	}

		void addElement(uint32_t stream, reng::Import::VertexElement& elem);

		void setData(uint32_t stream, reng::Import::VertexElement& elem, void* data, uint32_t srcStride);
		void setData(uint32_t stream, void* data);

		inline uint8_t *getData()			{ return m_data;		}
	protected:
		uint32_t	m_numVertices;
		uint8_t*	m_data;
	};

	class IonFbxHLSLMaterial
	{
	public:
		IonFbxHLSLMaterial();
		~IonFbxHLSLMaterial();
	};
#if 1
	class IonFbxMaterial //: public ion::Material
	{
	public:
		IonFbxMaterial() {m_flags = 0;}
		~IonFbxMaterial() {}
		
		void SetName(std::string name)
		{
			m_name = name;
			m_hash = hashString(m_name.c_str()); // ion::HashedString(m_name.c_str());
		}
		
		void SetAmbientColor(float r, float g, float b)
		{
			m_ambient[0] = r;
			m_ambient[1] = g;
			m_ambient[2] = b;
		}

		void SetDiffuseColor(float r, float g, float b)
		{
			m_diffuse[0] = powf(r, 2.2f);
			m_diffuse[1] = powf(g, 2.2f);
			m_diffuse[2] = powf(b, 2.2f);
		}

		void SetSpecularColor(float r, float g, float b)
		{
			m_specular[0] = r;
			m_specular[1] = g;
			m_specular[2] = b;
		}
		
		void SetAlpha(float alpha)
		{
			m_alpha = alpha;
		}
		
		void SetSpecularExponent(float exponent)
		{
			m_specular[3] = exponent;
		}
		
		void SetDiffuseMap(IonFbxTexture *texture)
		{
			m_pDiffuseMap = texture;
			m_flags |= kMFDiffuseMap;
		}

		void SetNormalMap(IonFbxTexture *texture)
		{
			m_pNormalMap = texture;
			m_flags |= kMFNormalMap;
		}

		void SetHeightMap(IonFbxTexture *texture)
		{
			m_pHeightMap = texture;
			m_flags |= kMFHeightMap;
		}

		void SetEmissiveMap(IonFbxTexture *texture)
		{
			m_pEmissiveMap = texture;
			m_flags |= kMFEmissiveMap;
		}

		void SetAmbientMap(IonFbxTexture *texture)
		{
			m_pAmbientMap = texture;
			m_flags |= kMFAmbientMap;
		}

		void SetMetallicMap(IonFbxTexture* texture)
		{
			m_pMetallicMap = texture;
			m_flags |= kMFMetallicMap;
		}

		void SetRoughnessMap(IonFbxTexture* texture)
		{
			m_pRoughnessMap = texture;
			m_flags |= kMFRoughnessMap;
		}

		void SetTransparency(bool transparency)
		{
			m_flags |= (transparency == true) ? kMFTransparent : 0;
		}

		void SetSkinned(bool skinned)
		{
			m_flags |= (skinned == true) ? kMFSkinned : 0;
		}

		void SetPhysicallyBased(bool physiciallyBassed)
		{
			m_flags |= (physiciallyBassed == true) ? kMFPhysicallyBased : 0;
		}

		void SetFlags(uint32_t flags)
		{
			m_flags = flags;
		}

		void SetUVScale(float uScale, float vScale)
		{
			m_uvScale[0] = uScale;
			m_uvScale[1] = vScale;
		}
						
		inline std::string& GetName()	{	return m_name;	}
		inline uint32_t GetHash()		{	return m_hash;	}
				
		std::string			m_name;
		uint32_t			m_hash;			// Was HashedString
		
		float				m_diffuse[3];
		float				m_ambient[3];
		float				m_specular[4];
		float				m_alpha;
		float				m_uvScale[2];
		uint32_t			m_flags;
		IonFbxTexture*		m_pDiffuseMap;
		IonFbxTexture*		m_pNormalMap;
		IonFbxTexture*		m_pHeightMap;
		IonFbxTexture*		m_pEmissiveMap;
		IonFbxTexture*		m_pAmbientMap;
		IonFbxTexture*		m_pMetallicMap;
		IonFbxTexture*		m_pRoughnessMap;
	};

	class IonFbxTexture
	{
	public:
		IonFbxTexture(std::string name);
		std::string		GetName()	{	return m_name;	}
		uint32_t		GetHash()	{	return m_hash;	}
	private:
		std::string		m_name;
		uint32_t		m_hash;
	};
#endif

	class IonFbxBone
	{
	public: 
//		IonFbxBone();
//		~IonFbxBone();

		std::string	m_name;
		mat4x4		m_boneMatrix;
	};

	class IonFbxLight
	{
	public:
		std::string	m_name;
		mat4x4		m_transform;
		vec4		m_color;
	};

	struct AABB
	{
	public:
		AABB(vec4 min, vec4 max);
		AABB();

		void Reset(float x, float y, float z);
		void Reset(vec4 p);
		void Add(vec4  p);

		//		inline Point Min()		{	return m_min;	}
		//		inline Point Max()		{	return m_max;	}

		void Print();

		vec4 aabbMin;
		vec4 aabbMax;
	};

#if 1
	class IonFbxSubMesh
	{
	public:
		IonFbxSubMesh();
		~IonFbxSubMesh();
		
		inline void SetMaterial(IonFbxMaterial* pMaterial)			{	m_pMaterial = pMaterial;	}
		inline void	SetNumVertices(uint32_t num )					{	m_numVertices = num;		}
		inline void	SetNumIndices(uint32_t num )					{	m_numIndices = num;			}
		inline void SetIndexBuffer(reng::Import::IndexBuffer* indexBuffer)	{	m_indexBuffer = indexBuffer;	}
		inline AABB& GetAABB()										{	return m_aabb;				}
		inline uint32_t	GetNumVertices()							{	return m_numVertices;		}
		inline uint32_t	GetNumIndices()								{	return m_numIndices;		}
		inline IonFbxMaterial* GetMaterial()						{	return m_pMaterial;			}
//		inline ion::Import::VertexBuffer*	GetVertexBuffer() { return m_vertexBuffer; }
		inline VertexBuffer*				getVertexBuffer() { return m_vertexBuffer; }
		inline reng::Import::IndexBuffer*	GetIndexBuffer()		{	return m_indexBuffer;		}
		void WriteIndices(uint32_t* indices, uint32_t size, bool keepLocal=false);
		void DispatchImmediate();
		
	private:
		AABB						m_aabb;
		uint32_t					m_numVertices;
		uint32_t					m_numIndices;
		IonFbxMaterial*				m_pMaterial;
//		ion::Import::VertexBuffer*	m_vertexBuffer;
		VertexBuffer*				m_vertexBuffer;
		reng::Import::IndexBuffer*	m_indexBuffer;
	};
#endif

	template <typename T>
	class UniqueVertexElementArray
	{
	public:
		UniqueVertexElementArray() {};
		~UniqueVertexElementArray()
		{
			m_map.clear();
			m_vector.clear();
		}

		uint32_t addElement(T& element)
		{
			uint64_t hash = Hash64(&element, sizeof(T));
			return addElement(hash, element);
			/*
			std::unordered_map<uint32_t, uint32_t>::iterator find = m_map.find(hash);
			if (find != m_map.end())
			{
			//                    printf("Vertex already in uniqueVerts no need to add it again\n");
			return find->second;
			}
			else
			{
			//                  printf("Adding New Vertex\n");
			uint32_t index = m_vector.size();
			m_vector.push_back(element);
			m_map[hash] = index;
			return index;
			}
			*/
		}

		uint32_t addElement(uint64_t hash, T& element)
		{
			std::unordered_map<uint64_t, uint32_t>::iterator find = m_map.find(hash);
			if (find != m_map.end())
			{
				//                    printf("Vertex already in uniqueVerts no need to add it again\n");
				return find->second;
			}
			else
			{
				//                  printf("Adding New Vertex\n");
				uint32_t index = static_cast<uint32_t>(m_vector.size());
				m_vector.push_back(element);
				m_map[hash] = index;
				return index;
			}
		}

		T& getElement(uint32_t index)
		{
			if (index != 0xffffffff)
			{
				AssertMsg((index < m_vector.size()), "Index out of range.\n");
				return m_vector.at(index);
			}
			else
			{
				printf("Trying to read when index = -1.  Is this correct?\n");
				return m_vector.at(-1);
			}
		}

		size_t size()
		{
			return m_vector.size();
		}
	private:
		std::unordered_map<uint64_t, uint32_t> m_map;		//unordered map to check if hash of element 'key' exists, and keep track of index into:
		std::vector<T> m_vector;	//vector containing unique vertex element.
	};

	struct UniqueVertexElements
	{
		UniqueVertexElementArray<Vertex> uniqueNormals;
		UniqueVertexElementArray<TexCoord> uniqueUVs;
		UniqueVertexElementArray<Vertex> uniqueTangents;
		UniqueVertexElementArray<Vertex> uniqueBinormals;
	};

	class IonFbxMesh //: public ion::Model
	{
	public:
		IonFbxMesh();
		~IonFbxMesh();
		
		void		ConvertMesh(const char* name, FbxMatrix& pGlobalPosition, FbxMesh* pMesh);
		void		ConvertMeshMaterials(FbxMesh* pMesh, uint32_t numMaterials, IonFbxMaterial** materials, bool bSkinned);

		uint32_t	GenerateFatVerts(FbxMesh* pMesh, Vertex4Bone *pSrcVertices, uint32_t matId, UniqueVertexElements& vertexElements, std::map<uint64_t, FbxFatVert>& uniqueVerts);
		void		ReindexGeometry(FbxMesh *pMesh, Vertex4Bone *pSrcVertices, uint32_t *indices, uint32_t matId, UniqueVertexElements& vertexElements, std::map<uint64_t, uint32_t>&indexRemappingTable );
		void		CreateChildren(uint32_t numChildren);

		inline mat4x4&	GetWorldMatrix()						{ return m_worldMatrix;			}
		inline const char*		GetName()						{ return m_name.c_str();		}
		inline AABB&			GetAABB()						{ return m_aabb;				}		
		inline uint32_t			GetNumSubMeshes()				{ return m_numSubMeshes;		}
		inline IonFbxSubMesh*	GetSubMesh(uint32_t index)		{ return &m_subMeshes[index];	}
		inline uint32_t			GetNumChildren()				{ return m_numChildren;			}
		inline IonFbxMesh*		GetChild( uint32_t child )		{ return &m_pChildren[child];	}
		inline mat4x4*			GetInverseBindPose()			{ return m_inverseBindPose;		}
		inline mat4x4&			GetBone( int index )			{ return m_bonePalette[index];  }

		void		Skin();
//		void		Render();
		
	private:

		std::string			m_name;
		
		mat4x4				m_worldMatrix;
		AABB				m_aabb;
		
		uint32_t			m_numMaterials;
		IonFbxMaterial**	m_materials;

//		uint32_t			m_numTextures;
//		IonFbxTexture*		m_textures;
		
		uint32_t			m_numSubMeshes;
		IonFbxSubMesh*		m_subMeshes;
	
		uint32_t			m_numBones;
		mat4x4*				m_inverseBindPose;
		mat4x4*				m_bonePalette;
		
		uint32_t			m_numChildren;
		IonFbxMesh*			m_pChildren;
	};

	class IonFbxMeshInstance
	{
	public:
		std::string name;
		IonFbxMesh *mesh;
		mat4x4 worldMatrix; // transform?
	};

	inline uint64_t Hash64(const void* data, int32_t length)
	{
		return MurmurHash64A(data, length, 0xdeadbeef);
	}
};

