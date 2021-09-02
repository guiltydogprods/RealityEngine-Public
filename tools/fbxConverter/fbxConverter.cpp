/*
 *  FbxImporter.cpp
 *  ionEngine
 *
 *  Created by Claire Rogers on 03/04/2010.
 *  Copyright 2010 Spinning Head Software. All rights reserved.
 *
 */

#include "stdafx.h"
#include "fbxConverter.h"
//#include "ion/Gfx/GfxDevice.h"
//#include "ion/Gfx/Material.h"
//#include "ion/Gfx/MaterialManager.h"
//#include "ion/Gfx/TextureManager.h"
//#include "ion/System/Assert.h"
#include "VCache.h"

//#include "ion/Resource/MeshExporter.h"


#include <fcntl.h>
//#include <unistd.h>
#include <io.h>

extern inline void rsys_print(const char* fmt, ...);
extern inline void rsys_assertFunc(const char* func, const char* file, int line, const char* e);
extern inline void rsys_assertMsgFunc(const char* func, const char* file, int line, const char* fmt, ...);

FbxMatrix GetGlobalPosition(FbxNode* pNode,
							  FbxTime& pTime,
							  FbxMatrix* pParentGlobalPosition = NULL);

FbxMatrix GetGlobalPosition(FbxNode* pNode, 
							  FbxTime& pTime, 
							  FbxPose* pPose,
							  FbxMatrix* pParentGlobalPosition = NULL);
FbxMatrix GetPoseMatrix(FbxPose* pPose, 
                          int pNodeIndex);
FbxMatrix GetGeometry(FbxNode* pNode);

// Get the global position.
// Do not take in account the geometric transform.
FbxMatrix GetGlobalPosition(FbxNode* pNode, FbxTime& pTime, FbxMatrix* pParentGlobalPosition)
{
	// Ideally this function would use parent global position and local position to
	// compute the global position.
	// Unfortunately the equation 
	//    lGlobalPosition = pParentGlobalPosition * lLocalPosition
	// does not hold when inheritance type is other than "Parent" (RSrs). To compute
	// the parent rotation and scaling is tricky in the RrSs and Rrs cases.
	// This is why GetGlobalFromCurrentTake() is used: it always computes the right
	// global position.
	
//	return pNode->GetGlobalFromCurrentTake(pTime);
	return g_pSceneEvaluator->GetNodeGlobalTransform(pNode, pTime);	
}


// Get the global position of the node for the current pose.
// If the specified node is not part of the pose, get its
// global position at the current time.
FbxMatrix GetGlobalPosition(FbxNode* pNode, FbxTime& pTime, FbxPose* pPose, FbxMatrix* pParentGlobalPosition)
{
	FbxMatrix lGlobalPosition;
	bool        lPositionFound = false;
	
	if (pPose)
	{
		int lNodeIndex = pPose->Find(pNode);
		
		if (lNodeIndex > -1)
		{
			// The bind pose is always a global matrix.
			// If we have a rest pose, we need to check if it is
			// stored in global or local space.
			if (pPose->IsBindPose() || 	!pPose->IsLocalMatrix(lNodeIndex))
			{
				lGlobalPosition = GetPoseMatrix(pPose, lNodeIndex);
			}
			else
			{
				// We have a local matrix, we need to convert it to
				// a global space matrix.
				FbxMatrix lParentGlobalPosition;
				
				if (pParentGlobalPosition)
				{
					lParentGlobalPosition = *pParentGlobalPosition;
				}
				else
				{
					if (pNode->GetParent())
					{
						lParentGlobalPosition = GetGlobalPosition(pNode->GetParent(), pTime, pPose);
					}
				}
				
				FbxMatrix lLocalPosition = GetPoseMatrix(pPose, lNodeIndex);
				lGlobalPosition = lParentGlobalPosition * lLocalPosition;
			}
			
			lPositionFound = true;
		}
	}
	
	if (!lPositionFound)
	{
		// There is no pose entry for that node, get the current global position instead
		lGlobalPosition = GetGlobalPosition(pNode, pTime, pParentGlobalPosition);
	}
	
	return lGlobalPosition;
}

// Get the matrix of the given pose
FbxMatrix GetPoseMatrix(FbxPose* pPose, int pNodeIndex)
{
	FbxMatrix lPoseMatrix;
	FbxMatrix lMatrix = pPose->GetMatrix(pNodeIndex);
	
	memcpy((double*)lPoseMatrix, (double*)lMatrix, sizeof(lMatrix.mData));
	
	return lPoseMatrix;
}

// Get the geometry deformation local to a node. It is never inherited by the
// children.
/*
KFbxXMatrix GetGeometry(KFbxNode* pNode) {
	KFbxVector4 lT, lR, lS;
	KFbxXMatrix lGeometry;
	
	lT = pNode->GetGeometricTranslation(KFbxNode::eSOURCE_SET);
	lR = pNode->GetGeometricRotation(KFbxNode::eSOURCE_SET);
	lS = pNode->GetGeometricScaling(KFbxNode::eSOURCE_SET);
	
	lGeometry.SetT(lT);
	lGeometry.SetR(lR);
	lGeometry.SetS(lS);
	
	return lGeometry;
}
*/

void DisplayString(const char* pHeader, const char* pValue = "", const char* pSuffix = "");
void DisplayBool(const char* pHeader, bool pValue, const char* pSuffix = "");
void DisplayInt(const char* pHeader, int pValue, const char* pSuffix = "");
void DisplayDouble(const char* pHeader, double pValue, const char* pSuffix = "");
void Display2DVector(const char* pHeader, FbxVector2 pValue, const char* pSuffix = "");
void Display3DVector(const char* pHeader, FbxVector4 pValue, const char* pSuffix = "");
void DisplayColor(const char* pHeader, FbxColor pValue, const char* pSuffix = "");
void Display4DVector(const char* pHeader, FbxVector4 pValue, const char* pSuffix = "");

void DisplayString(const char* pHeader, const char* pValue /* = "" */, const char* pSuffix /* = "" */)
{
	FbxString lString;

	lString = pHeader;
	lString += pValue;
	lString += pSuffix;
	lString += "\n";
	FBXSDK_printf(lString);
}


void DisplayBool(const char* pHeader, bool pValue, const char* pSuffix /* = "" */)
{
	FbxString lString;

	lString = pHeader;
	lString += pValue ? "true" : "false";
	lString += pSuffix;
	lString += "\n";
	FBXSDK_printf(lString);
}


void DisplayInt(const char* pHeader, int pValue, const char* pSuffix /* = "" */)
{
	FbxString lString;

	lString = pHeader;
	lString += pValue;
	lString += pSuffix;
	lString += "\n";
	FBXSDK_printf(lString);
}


void DisplayDouble(const char* pHeader, double pValue, const char* pSuffix /* = "" */)
{
	FbxString lString;
	FbxString lFloatValue = (float)pValue;

	lFloatValue = pValue <= -HUGE_VAL ? "-INFINITY" : lFloatValue.Buffer();
	lFloatValue = pValue >= HUGE_VAL ? "INFINITY" : lFloatValue.Buffer();

	lString = pHeader;
	lString += lFloatValue;
	lString += pSuffix;
	lString += "\n";
	FBXSDK_printf(lString);
}


void Display2DVector(const char* pHeader, FbxVector2 pValue, const char* pSuffix  /* = "" */)
{
	FbxString lString;
	FbxString lFloatValue1 = (float)pValue[0];
	FbxString lFloatValue2 = (float)pValue[1];

	lFloatValue1 = pValue[0] <= -HUGE_VAL ? "-INFINITY" : lFloatValue1.Buffer();
	lFloatValue1 = pValue[0] >= HUGE_VAL ? "INFINITY" : lFloatValue1.Buffer();
	lFloatValue2 = pValue[1] <= -HUGE_VAL ? "-INFINITY" : lFloatValue2.Buffer();
	lFloatValue2 = pValue[1] >= HUGE_VAL ? "INFINITY" : lFloatValue2.Buffer();

	lString = pHeader;
	lString += lFloatValue1;
	lString += ", ";
	lString += lFloatValue2;
	lString += pSuffix;
	lString += "\n";
	FBXSDK_printf(lString);
}


void Display3DVector(const char* pHeader, FbxVector4 pValue, const char* pSuffix /* = "" */)
{
	FbxString lString;
	FbxString lFloatValue1 = (float)pValue[0];
	FbxString lFloatValue2 = (float)pValue[1];
	FbxString lFloatValue3 = (float)pValue[2];

	lFloatValue1 = pValue[0] <= -HUGE_VAL ? "-INFINITY" : lFloatValue1.Buffer();
	lFloatValue1 = pValue[0] >= HUGE_VAL ? "INFINITY" : lFloatValue1.Buffer();
	lFloatValue2 = pValue[1] <= -HUGE_VAL ? "-INFINITY" : lFloatValue2.Buffer();
	lFloatValue2 = pValue[1] >= HUGE_VAL ? "INFINITY" : lFloatValue2.Buffer();
	lFloatValue3 = pValue[2] <= -HUGE_VAL ? "-INFINITY" : lFloatValue3.Buffer();
	lFloatValue3 = pValue[2] >= HUGE_VAL ? "INFINITY" : lFloatValue3.Buffer();

	lString = pHeader;
	lString += lFloatValue1;
	lString += ", ";
	lString += lFloatValue2;
	lString += ", ";
	lString += lFloatValue3;
	lString += pSuffix;
	lString += "\n";
	FBXSDK_printf(lString);
}

void Display4DVector(const char* pHeader, FbxVector4 pValue, const char* pSuffix /* = "" */)
{
	FbxString lString;
	FbxString lFloatValue1 = (float)pValue[0];
	FbxString lFloatValue2 = (float)pValue[1];
	FbxString lFloatValue3 = (float)pValue[2];
	FbxString lFloatValue4 = (float)pValue[3];

	lFloatValue1 = pValue[0] <= -HUGE_VAL ? "-INFINITY" : lFloatValue1.Buffer();
	lFloatValue1 = pValue[0] >= HUGE_VAL ? "INFINITY" : lFloatValue1.Buffer();
	lFloatValue2 = pValue[1] <= -HUGE_VAL ? "-INFINITY" : lFloatValue2.Buffer();
	lFloatValue2 = pValue[1] >= HUGE_VAL ? "INFINITY" : lFloatValue2.Buffer();
	lFloatValue3 = pValue[2] <= -HUGE_VAL ? "-INFINITY" : lFloatValue3.Buffer();
	lFloatValue3 = pValue[2] >= HUGE_VAL ? "INFINITY" : lFloatValue3.Buffer();
	lFloatValue4 = pValue[3] <= -HUGE_VAL ? "-INFINITY" : lFloatValue4.Buffer();
	lFloatValue4 = pValue[3] >= HUGE_VAL ? "INFINITY" : lFloatValue4.Buffer();

	lString = pHeader;
	lString += lFloatValue1;
	lString += ", ";
	lString += lFloatValue2;
	lString += ", ";
	lString += lFloatValue3;
	lString += ", ";
	lString += lFloatValue4;
	lString += pSuffix;
	lString += "\n";
	FBXSDK_printf(lString);
}

namespace rengineTools
{
	Vertex::Vertex()
	{
	}
	
	Vertex::Vertex( float _x, float _y, float _z )
	{
		x = _x;
		y = _y;
		z = _z;
	}

	TexCoord::TexCoord()
	{
	}
	
	TexCoord::TexCoord( float _u, float _v )
	{
		u = _u;
		v = _v;
	}

	Vector4i::Vector4i() {}
	Vector4i::Vector4i(int32_t _x, int32_t _y, int32_t _z, int32_t _w)
	{
		x = _x;
		y = _y;
		z = _z;
		w = _w;
	}

	Vector4f::Vector4f() {}
	Vector4f::Vector4f(float _x, float _y, float _z, float _w)
	{
		x = _x;
		y = _y;
		z = _z;
		w = _w;
	}

	bool			   _bStoreHashes;
//	std::set<uint32_t> _textureHashes;
	std::vector<IonFbxLight> _lights;
	std::set<IonFbxTexture*>	_textures;
	std::set<uint32_t> _materialHashes;
	std::set<IonFbxMaterial*>	_materials;
	std::vector<IonFbxBone>	_bones;
	
	FbxConverter::FbxConverter()
		: m_frameRate(0.0f)
		, m_numTakes(0)
		, m_firstFrame(true)
		, m_pTakes(nullptr)
		, m_numMeshes(0)
		, m_pMeshes(nullptr)
		, m_isSkinned(false)
	{
		m_pSdkManager = FbxManager::Create();		
		
		if (!m_pSdkManager)
		{
			printf("Unable to create the FBX SDK manager\n");
			exit(0);
		}
		
		// Load plugins from the executable directory
		FbxString lPath = FbxGetApplicationDirectory();
		FbxString lExtension = "dylib";
		m_pSdkManager->LoadPluginsDirectory(lPath.Buffer(), lExtension.Buffer());
		
		// Create the entity that will hold the scene.
		m_pScene = FbxScene::Create(m_pSdkManager,"");
		if (!m_pScene)
		{
			printf("Unable to create empty scene\n");
			exit(0);
		}
	}

	FbxConverter::~FbxConverter()
	{
	}

	//Public exposed method
	void FbxConverter::ImportFile(std::string filename, std::vector<IonFbxMesh*>& scene)
	{
		ImportFile(filename, scene, false);
	}
	
	void FbxConverter::ImportFile(std::string filename, std::vector<IonFbxMesh*>& scene, bool bStoreHashes)
	{
		if (m_pSdkManager)
		{			
			// Create the importer.
			int lFileFormat = -1;
			
			m_pImporter = FbxImporter::Create(m_pSdkManager,"");
			if (!m_pSdkManager->GetIOPluginRegistry()->DetectReaderFileFormat(filename.c_str(), lFileFormat))
			{
				// Unrecognizable file format. Try to fall back to KFbxImporter::eFBX_BINARY
				lFileFormat = m_pSdkManager->GetIOPluginRegistry()->FindReaderIDByDescription( "FBX binary (*.fbx)" );;
			}
//			m_pImporter->SetFileFormat(lFileFormat);
			
			// Initialize the importer by providing a filename.
			if(m_pImporter->Initialize(filename.c_str()) == true)
			{
                if (m_pImporter->Import(m_pScene) == true)
                {
					printf("Import: %s\n", filename.c_str());
					g_pSceneEvaluator = m_pScene->GetAnimationEvaluator();
					Convert();			//Convert to triangles
					uint32_t takeCount = m_numTakes = m_pImporter->GetAnimStackCount();
					m_pTakes = new IonFbxTake[takeCount];
					double fDuration = 0.0f;
					if (takeCount != 0)
					{
						for (uint32_t take = 0; take < takeCount; ++take)
						{
							FbxTakeInfo* info = m_pImporter->GetTakeInfo(0);
							const char* takeName = info->mName.Buffer();
							FbxTime duration = info->mLocalTimeSpan.GetDuration();
							FbxTime start = info->mLocalTimeSpan.GetStart();
							FbxTime stop = info->mLocalTimeSpan.GetStop();
							fDuration = duration.GetSecondDouble();
							m_frameRate = FbxTime::GetFrameRate(FbxTime::eDefaultMode);
							uint32_t numFrames = /*fDuration == 1.0f ? 1 :*/ (uint32_t)duration.GetFrameCount(FbxTime::eDefaultMode);	//, KTime::eDefaultMode, 0.0f);
//							double numFrames = duration.GetFrameCountPrecise();
//							m_pPoses = new IonFbxPose[m_numFrames];
							m_pTakes[take].numFrames = numFrames;
							m_pTakes[take].frames = new IonFbxPose[numFrames];
							m_pTakes[take].name = takeName;
							double frameTime = start.GetSecondDouble(); // .0f;
							m_startTime = frameTime;
							double endFrameTime = stop.GetSecondDouble();
							double frameTimeDelta = 1.0 / m_frameRate;
							m_pTakes[take].startTime = (float)frameTime;
							m_pTakes[take].endTime = (float)endFrameTime;
							m_currentTakeIndex = take;
							for (uint32_t i = 0; i < numFrames; ++i)
							{
								m_currentFrameIndex = i;
								ExtractPoseAtTime((float)frameTime);
								frameTime += frameTimeDelta;
								m_firstFrame = false;
							}
						}
					}
					else
					{
						m_frameRate = 0;
						m_numTakes = 0;
						m_pTakes = nullptr;
						ExtractPoseAtTime(0.0f);
					}
					
					m_pImporter->Destroy();
					
//					printf("%d frames, %.3f seconds, %.1ffps\n", m_numFrames, fDuration, m_frameRate);
					
					for (uint32_t i = 0; i < m_numMeshes; ++i)
					{
						if (m_pMeshes[i].GetNumSubMeshes() > 0)
						{
							scene.push_back(&m_pMeshes[i]);
						}
					}
					m_numMeshes = 0;
					m_pMeshes = 0;
				}
			}
			else
			{
				//				*gWindowMessage = "Unable to open file ";
			}
		}
		else
		{
			//			*gWindowMessage = "Unable to create the FBX SDK manager";
		}
//		return 0;
	}
	
	void FbxConverter::ConvertFile(std::string inputFilename, std::string outputFilename)
	{
		std::vector<IonFbxMesh*> models;
		ImportFile(inputFilename, models, true);
		printf("Model count: %d\n", (int)models.size());
		
		Export(outputFilename, models);
		models.clear();
		if (1)	//_bStoreHashes) {
		{
//			_textureHashes.clear();
			_textures.clear();
//			_materialHashes.clear();
//			_materials.clear();
		}
	}
	
	void FbxConverter::Convert()
	{
		// Convert Axis System to what is used in this example, if needed
		FbxAxisSystem SceneAxisSystem = m_pScene->GetGlobalSettings().GetAxisSystem();
		FbxAxisSystem OurAxisSystem(FbxAxisSystem::eYAxis, FbxAxisSystem::eParityOdd, FbxAxisSystem::eRightHanded);
		if( SceneAxisSystem != OurAxisSystem )
		{
			OurAxisSystem.ConvertScene(m_pScene);
		}

		// Convert Unit System to what is used in this example, if needed
		FbxSystemUnit SceneSystemUnit = m_pScene->GetGlobalSettings().GetSystemUnit();

		if (SceneSystemUnit.GetScaleFactor() != 1.0)
		{
			//The unit in this example is centimeter.
//			FbxSystemUnit::cm.ConvertScene(m_pScene);
//			SceneSystemUnit.m.ConvertScene(m_pScene);
		}

		// Nurbs and patch attribute types are not supported yet.
		// Convert them into mesh node attributes to have them drawn.
		ConvertNurbsAndPatchRecursive(m_pSdkManager, m_pScene->GetRootNode());

	}

	void FbxConverter::ConvertNurbsAndPatchRecursive(FbxManager* pSdkManager, FbxNode* pNode)
	{
		FbxNodeAttribute* lNodeAttribute = pNode->GetNodeAttribute();
		
		if (lNodeAttribute)
		{
			if (lNodeAttribute->GetAttributeType() == FbxNodeAttribute::eNurbs ||
				lNodeAttribute->GetAttributeType() == FbxNodeAttribute::ePatch ||
				lNodeAttribute->GetAttributeType() == FbxNodeAttribute::ePatch ||
				lNodeAttribute->GetAttributeType() == FbxNodeAttribute::eMesh)
			{
				FbxGeometryConverter lConverter(pSdkManager);
				lConverter.Triangulate(lNodeAttribute, true);
			}
		}
		
		int i, lCount = pNode->GetChildCount();
		
		for (i = 0; i < lCount; i++)
		{
			ConvertNurbsAndPatchRecursive(pSdkManager, pNode->GetChild(i));
		}
	}
				
	void FbxConverter::ExtractPoseAtTime(float time)
	{
        FbxTime internalTime;
        internalTime.SetSecondDouble(time);
        FbxMatrix lDummyGlobalPosition;
		FbxNode* pNode = m_pScene->GetRootNode();
		int nodeLevel = 0;
		printf("TypeName: %s\n", pNode->GetTypeName());
		int totalChildNodes = m_pScene->GetRootNode()->GetChildCount(true);
		int numChildNodes = m_pScene->GetRootNode()->GetChildCount();
		if (m_firstFrame)
		{
			m_numMeshes = 0;	// numChildNodes;
			m_pMeshes = new IonFbxMesh[totalChildNodes];	//[m_numMeshes];
			memset(m_pMeshes, 0, sizeof(IonFbxMesh) * totalChildNodes);
		}
        for( int i = 0; i < numChildNodes; ++i )
        {
			FbxNode* pNode = m_pScene->GetRootNode()->GetChild(i);
			printf("TypeName: %s\n", pNode->GetTypeName());
//			if (!strcmp("Mesh", pNode->GetTypeName()))
//			{
				ExtractPoseAtTimeRecursive(pNode, internalTime, lDummyGlobalPosition, &m_pMeshes[i], nodeLevel);
//				if (!strcmp("Mesh", pNode->GetTypeName()))
//	 			m_numMeshes++;
//			}
        }	
		m_numMeshes = totalChildNodes;	//CLR - Actually the total number of nodes.  Not actually numMeshes.
	}
	
    void FbxConverter::ExtractPoseAtTimeRecursive(FbxNode* pNode,
										FbxTime& pTime, 
										FbxMatrix& pParentGlobalPosition, IonFbxMesh* pMesh, int nodeLevel)
    {
        // Compute the node's global position.
		//        KFbxXMatrix lGlobalPosition = GetGlobalPosition(pNode, pTime, &pParentGlobalPosition);
//		FbxMatrix lGlobalPosition = g_pSceneEvaluator->GetNodeGlobalTransform(pNode, pTime);
//		FbxMatrix lGlobalTransform = pNode->EvaluateGlobalTransform();
		
		// Get the node's default local transformation matrix.
		FbxMatrix lLocalTransform = pNode->EvaluateLocalTransform();		//pNode->GetGlobalFromCurrentTake(pTime);
				
        // Geometry offset.
        // it is not inherited by the children.
//        KFbxXMatrix lGeometryOffset = GetGeometry(pNode);						//CLR: Not sure what GeometryOffset is.
//        KFbxXMatrix lGlobalOffPosition = lGlobalPosition * lGeometryOffset;
		const char* nodeName = pNode->GetName();
		printf("Node: %s (%d)\n", nodeName, nodeLevel);
        ExtractPose(pNode, pTime, pParentGlobalPosition, lLocalTransform, pMesh);
        
        int childCount = pNode->GetChildCount();
		if (childCount > 0)
		{
			pMesh->CreateChildren(childCount);
			for (int i = 0; i < childCount; i++)
			{
				IonFbxMesh* pChild = static_cast<IonFbxMesh*>(pMesh->GetChild(i));
				ExtractPoseAtTimeRecursive(pNode->GetChild(i), pTime, lLocalTransform, pChild, nodeLevel+1);
			}			
		}
        
    }    
	
    void FbxConverter::ExtractPose(FbxNode* pNode, FbxTime& pTime, FbxMatrix& pParentGlobalPosition, FbxMatrix& pGlobalPosition, IonFbxMesh* pMesh)
    {
//		ion::Matrix44 scaleMat(ion::kIdentity);
//		scaleMat.SetScale(0.01f);
		mat4x4 scaleMat = mat4x4_scale(0.01f);
/*
		ion::Matrix44 parentMatrix;
		parentMatrix.SetAxisX(ion::Vector4((float)pParentGlobalPosition.Get(0, 0), (float)pParentGlobalPosition.Get(0, 1), (float)pParentGlobalPosition.Get(0, 2), (float)pParentGlobalPosition.Get(0, 3)));
		parentMatrix.SetAxisY(ion::Vector4((float)pParentGlobalPosition.Get(1, 0), (float)pParentGlobalPosition.Get(1, 1), (float)pParentGlobalPosition.Get(1, 2), (float)pParentGlobalPosition.Get(1, 3)));
		parentMatrix.SetAxisZ(ion::Vector4((float)pParentGlobalPosition.Get(2, 0), (float)pParentGlobalPosition.Get(2, 1), (float)pParentGlobalPosition.Get(2, 2), (float)pParentGlobalPosition.Get(2, 3)));
		parentMatrix.SetAxisW(ion::Vector4((float)pParentGlobalPosition.Get(3, 0), (float)pParentGlobalPosition.Get(3, 1), (float)pParentGlobalPosition.Get(3, 2), (float)pParentGlobalPosition.Get(3, 3)));
		parentMatrix.SetAxisW(scaleMat * parentMatrix.GetAxisW());
*/
		mat4x4 parentMatrix;
		parentMatrix.xAxis = (vec4){ (float)pParentGlobalPosition.Get(0, 0), (float)pParentGlobalPosition.Get(0, 1), (float)pParentGlobalPosition.Get(0, 2), (float)pParentGlobalPosition.Get(0, 3) };
		parentMatrix.yAxis = (vec4){ (float)pParentGlobalPosition.Get(1, 0), (float)pParentGlobalPosition.Get(1, 1), (float)pParentGlobalPosition.Get(1, 2), (float)pParentGlobalPosition.Get(1, 3) };
		parentMatrix.zAxis = (vec4){ (float)pParentGlobalPosition.Get(2, 0), (float)pParentGlobalPosition.Get(2, 1), (float)pParentGlobalPosition.Get(2, 2), (float)pParentGlobalPosition.Get(2, 3) };
		parentMatrix.wAxis = vec4_transform(scaleMat, (vec4) { (float)pParentGlobalPosition.Get(3, 0), (float)pParentGlobalPosition.Get(3, 1), (float)pParentGlobalPosition.Get(3, 2), (float)pParentGlobalPosition.Get(3, 3) });
		const char* nodeName = pNode->GetName();

        FbxNodeAttribute* lNodeAttribute = pNode->GetNodeAttribute();
        if (lNodeAttribute)
		{
			uint32_t nodeType = (uint32_t)lNodeAttribute->GetAttributeType();
            switch (lNodeAttribute->GetAttributeType() )
            {
                case FbxNodeAttribute::eSkeleton:
				{
#if 0
					FbxSkeleton *lSkeleton = (FbxSkeleton *)pNode->GetNodeAttribute();
					if (lSkeleton->GetSkeletonType() == FbxSkeleton::eLimbNode) // &&
//						pNode->GetParent() &&
//						pNode->GetParent()->GetNodeAttribute() &&
//						pNode->GetParent()->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton)
					{
//						GlDrawLimbNode(pParentGlobalPosition, pGlobalPosition);
						FbxMatrix nodeTransformMatrix = pGlobalPosition;

						ion::Matrix44 nodeMatrix;
						nodeMatrix.SetAxisX(ion::Vector4((float)nodeTransformMatrix.Get(0, 0), (float)nodeTransformMatrix.Get(0, 1), (float)nodeTransformMatrix.Get(0, 2), (float)nodeTransformMatrix.Get(0, 3)));
						nodeMatrix.SetAxisY(ion::Vector4((float)nodeTransformMatrix.Get(1, 0), (float)nodeTransformMatrix.Get(1, 1), (float)nodeTransformMatrix.Get(1, 2), (float)nodeTransformMatrix.Get(1, 3)));
						nodeMatrix.SetAxisZ(ion::Vector4((float)nodeTransformMatrix.Get(2, 0), (float)nodeTransformMatrix.Get(2, 1), (float)nodeTransformMatrix.Get(2, 2), (float)nodeTransformMatrix.Get(2, 3)));
						nodeMatrix.SetAxisW(ion::Vector4((float)nodeTransformMatrix.Get(3, 0), (float)nodeTransformMatrix.Get(3, 1), (float)nodeTransformMatrix.Get(3, 2), (float)nodeTransformMatrix.Get(3, 3)));
						nodeMatrix.SetAxisW(scaleMat * nodeMatrix.GetAxisW());
//						_bones.push_back(nodeMatrix);
						IonFbxBone newBone;
						newBone.m_name = nodeName;
						newBone.m_boneMatrix = nodeMatrix;
						_bones.push_back(newBone);
//						ion::PrintMatrix("Matrix", nodeMatrix);
						ion::PrintMatrix("Final Matrix", OrthoInverse(parentMatrix) * nodeMatrix);
						//						char nodeDesc[128];
//						sprintf(nodeDesc, "Node %s", nodeName);
					}
#endif
					break;
				}
                case FbxNodeAttribute::eMesh:
                {
                    FbxMesh *pFbxMesh = (FbxMesh *)pNode->GetNodeAttribute(); 
					int polyCount = pFbxMesh->GetPolygonCount();
					if (polyCount)
                    {
						if (m_firstFrame == true)
						{
							std::unordered_map<FbxMesh *, IonFbxMesh *>::iterator find = m_meshes.find(pFbxMesh);
							if (find == m_meshes.end())
							{
								m_meshes[pFbxMesh] = pMesh;
								pMesh->ConvertMesh(nodeName, pGlobalPosition, pFbxMesh);
							}
							pMesh = m_meshes[pFbxMesh];
							FbxMatrix nodeTransformMatrix = pGlobalPosition;
/*
							ion::Matrix44 nodeMatrix;
							nodeMatrix.SetAxisX(ion::Vector4((float)nodeTransformMatrix.Get(0, 0), (float)nodeTransformMatrix.Get(0, 1), (float)nodeTransformMatrix.Get(0, 2), (float)nodeTransformMatrix.Get(0, 3)));
							nodeMatrix.SetAxisY(ion::Vector4((float)nodeTransformMatrix.Get(1, 0), (float)nodeTransformMatrix.Get(1, 1), (float)nodeTransformMatrix.Get(1, 2), (float)nodeTransformMatrix.Get(1, 3)));
							nodeMatrix.SetAxisZ(ion::Vector4((float)nodeTransformMatrix.Get(2, 0), (float)nodeTransformMatrix.Get(2, 1), (float)nodeTransformMatrix.Get(2, 2), (float)nodeTransformMatrix.Get(2, 3)));
							nodeMatrix.SetAxisW(ion::Vector4((float)nodeTransformMatrix.Get(3, 0), (float)nodeTransformMatrix.Get(3, 1), (float)nodeTransformMatrix.Get(3, 2), (float)nodeTransformMatrix.Get(3, 3)));

							ion::Matrix44 scaleMat(ion::kIdentity);
							scaleMat.SetScale(0.01f);
*/
							mat4x4 nodeMatrix;
							nodeMatrix.xAxis = (vec4){ (float)nodeTransformMatrix.Get(0, 0), (float)nodeTransformMatrix.Get(0, 1), (float)nodeTransformMatrix.Get(0, 2), (float)nodeTransformMatrix.Get(0, 3) };
							nodeMatrix.yAxis = (vec4){ (float)nodeTransformMatrix.Get(1, 0), (float)nodeTransformMatrix.Get(1, 1), (float)nodeTransformMatrix.Get(1, 2), (float)nodeTransformMatrix.Get(1, 3) };
							nodeMatrix.zAxis = (vec4){ (float)nodeTransformMatrix.Get(2, 0), (float)nodeTransformMatrix.Get(2, 1), (float)nodeTransformMatrix.Get(2, 2), (float)nodeTransformMatrix.Get(2, 3) };
							nodeMatrix.wAxis = (vec4){ (float)nodeTransformMatrix.Get(3, 0), (float)nodeTransformMatrix.Get(3, 1), (float)nodeTransformMatrix.Get(3, 2), (float)nodeTransformMatrix.Get(3, 3) };

							IonFbxMeshInstance *inst = new IonFbxMeshInstance;
							inst->name = nodeName;
							inst->mesh = pMesh;
							inst->worldMatrix.xAxis = nodeMatrix.xAxis;
							inst->worldMatrix.yAxis = nodeMatrix.yAxis;
							inst->worldMatrix.zAxis = nodeMatrix.zAxis;
							inst->worldMatrix.wAxis = vec4_transform(scaleMat, nodeMatrix.wAxis);
							m_meshInstances.push_back(inst);
						}
						ExtractPoseBones(pNode, pTime, pGlobalPosition);
                    }
                    break;   
                }
				case FbxNodeAttribute::eLight:
				{
					if (m_firstFrame)
					{
						FbxLight* pFbxLight = (FbxLight*)pNode->GetNodeAttribute();
						FbxLight::EType lightType = pFbxLight->LightType.Get();
						FbxDouble3 lightColor = pFbxLight->Color.Get();

						float lightR = (float)lightColor[0];
						float lightG = (float)lightColor[1];
						float lightB = (float)lightColor[2];

						FbxMatrix nodeTransformMatrix = pGlobalPosition;

//						ion::Matrix44 nodeMatrix;
//						nodeMatrix.SetAxisX(ion::Vector4((float)nodeTransformMatrix.Get(0, 0), (float)nodeTransformMatrix.Get(0, 1), (float)nodeTransformMatrix.Get(0, 2), (float)nodeTransformMatrix.Get(0, 3)));
//						nodeMatrix.SetAxisY(ion::Vector4((float)nodeTransformMatrix.Get(1, 0), (float)nodeTransformMatrix.Get(1, 1), (float)nodeTransformMatrix.Get(1, 2), (float)nodeTransformMatrix.Get(1, 3)));
//						nodeMatrix.SetAxisZ(ion::Vector4((float)nodeTransformMatrix.Get(2, 0), (float)nodeTransformMatrix.Get(2, 1), (float)nodeTransformMatrix.Get(2, 2), (float)nodeTransformMatrix.Get(2, 3)));
//						nodeMatrix.SetAxisW(ion::Vector4((float)nodeTransformMatrix.Get(3, 0), (float)nodeTransformMatrix.Get(3, 1), (float)nodeTransformMatrix.Get(3, 2), (float)nodeTransformMatrix.Get(3, 3)));
//						nodeMatrix.SetAxisW(scaleMat * nodeMatrix.GetAxisW());
						mat4x4 nodeMatrix;
						nodeMatrix.xAxis = (vec4){ (float)nodeTransformMatrix.Get(0, 0), (float)nodeTransformMatrix.Get(0, 1), (float)nodeTransformMatrix.Get(0, 2), (float)nodeTransformMatrix.Get(0, 3) };
						nodeMatrix.yAxis = (vec4){ (float)nodeTransformMatrix.Get(1, 0), (float)nodeTransformMatrix.Get(1, 1), (float)nodeTransformMatrix.Get(1, 2), (float)nodeTransformMatrix.Get(1, 3) };
						nodeMatrix.zAxis = (vec4){ (float)nodeTransformMatrix.Get(2, 0), (float)nodeTransformMatrix.Get(2, 1), (float)nodeTransformMatrix.Get(2, 2), (float)nodeTransformMatrix.Get(2, 3) };
						nodeMatrix.wAxis = vec4_transform(scaleMat, (vec4){ (float)nodeTransformMatrix.Get(3, 0), (float)nodeTransformMatrix.Get(3, 1), (float)nodeTransformMatrix.Get(3, 2), (float)nodeTransformMatrix.Get(3, 3) });

						IonFbxLight newLight;
						newLight.m_name = nodeName;
						newLight.m_transform = nodeMatrix;
//						newLight.m_color = ion::Vector(lightR, lightG, lightB);
						newLight.m_color = (vec4){ lightR, lightG, lightB, 0.0f };
						_lights.push_back(newLight);
					}
					break;
				}
                default:
                    break;
            }
		}
		else
		{
//			DrawNull(pGlobalPosition);
		}
	}
	
	void FbxConverter::ExtractPoseBones(FbxNode* pNode, FbxTime& pTime, FbxMatrix& pGlobalPosition)
	{
//		ion::Matrix44 scaleMat(ion::kIdentity);
//		scaleMat.SetScale(0.01f);
		mat4x4 scaleMat = mat4x4_scale(0.01f);

        FbxMesh* lMesh = (FbxMesh*) pNode->GetNodeAttribute();
        int lClusterCount = 0;
        int lSkinCount= 0;
        int lVertexCount = lMesh->GetControlPointsCount();
        
        // No vertex to draw.
        if (lVertexCount == 0)
        {
            return;
        }
		
        // Active vertex cache deformer will overwrite any other deformer
		if (lMesh->GetDeformerCount(FbxDeformer::eVertexCache) &&
			(static_cast<FbxVertexCacheDeformer*>(lMesh->GetDeformer(0, FbxDeformer::eVertexCache)))->Active.Get())
        {
            printf("ReadVertexCacheData not supported\n");
        }
        else
        {
            //we need to get the number of clusters
            lSkinCount = lMesh->GetDeformerCount(FbxDeformer::eSkin);
            for( int i=0; i< lSkinCount; i++)
                lClusterCount += ((FbxSkin *)(lMesh->GetDeformer(i, FbxDeformer::eSkin)))->GetClusterCount();
            if (lClusterCount)
            {
				FbxMatrix nodeTransformMatrix = pGlobalPosition;
				
//				ion::Matrix44 nodeMatrix(ion::kIdentity);
//				nodeMatrix.SetAxisX(ion::Vector4( (float)nodeTransformMatrix.Get(0, 0), (float)nodeTransformMatrix.Get(0, 1), (float)nodeTransformMatrix.Get(0, 2), (float)nodeTransformMatrix.Get(0, 3)));
//				nodeMatrix.SetAxisY(ion::Vector4( (float)nodeTransformMatrix.Get(1, 0), (float)nodeTransformMatrix.Get(1, 1), (float)nodeTransformMatrix.Get(1, 2), (float)nodeTransformMatrix.Get(1, 3)));
//				nodeMatrix.SetAxisZ(ion::Vector4( (float)nodeTransformMatrix.Get(2, 0), (float)nodeTransformMatrix.Get(2, 1), (float)nodeTransformMatrix.Get(2, 2), (float)nodeTransformMatrix.Get(2, 3)));
//				nodeMatrix.SetAxisW(ion::Vector4( (float)nodeTransformMatrix.Get(3, 0), (float)nodeTransformMatrix.Get(3, 1), (float)nodeTransformMatrix.Get(3, 2), (float)nodeTransformMatrix.Get(3, 3)));
				mat4x4 nodeMatrix = {
					.xAxis = (vec4){ (float)nodeTransformMatrix.Get(0, 0), (float)nodeTransformMatrix.Get(0, 1), (float)nodeTransformMatrix.Get(0, 2), (float)nodeTransformMatrix.Get(0, 3) },
					.yAxis = (vec4){ (float)nodeTransformMatrix.Get(1, 0), (float)nodeTransformMatrix.Get(1, 1), (float)nodeTransformMatrix.Get(1, 2), (float)nodeTransformMatrix.Get(1, 3) },
					.zAxis = (vec4){ (float)nodeTransformMatrix.Get(2, 0), (float)nodeTransformMatrix.Get(2, 1), (float)nodeTransformMatrix.Get(2, 2), (float)nodeTransformMatrix.Get(2, 3) },
					.wAxis = (vec4){ (float)nodeTransformMatrix.Get(3, 0), (float)nodeTransformMatrix.Get(3, 1), (float)nodeTransformMatrix.Get(3, 2), (float)nodeTransformMatrix.Get(3, 3) },
				};

				//Buildup Matrix Palette            
				lClusterCount =( (FbxSkin *)lMesh->GetDeformer(0, FbxDeformer::eSkin))->GetClusterCount();

//				int poseIndex = GetPose(pTime.GetSecondDouble());
				IonFbxPose* pose = m_pTakes[m_currentTakeIndex].frames + m_currentFrameIndex; // poseIndex;
				pose->m_numBones = lClusterCount;
				pose->m_bones = (mat4x4*)malloc(sizeof(mat4x4) * lClusterCount); // new ion::Matrix44[lClusterCount];
				for (int j=0; j<lClusterCount; ++j)
				{            
					FbxCluster* lCluster =((FbxSkin *) lMesh->GetDeformer(0, FbxDeformer::eSkin))->GetCluster(j);
					if (!lCluster->GetLink())
						continue;
					
					//Get Bone Matrix for current frame
					FbxMatrix fbxBoneMatrix = GetGlobalPosition(lCluster->GetLink(), pTime, &pGlobalPosition);
					//Convert to ion friendly format                        
/*
					ion::Matrix44& boneMatrix = pose->m_bones[j];
					boneMatrix.SetAxisX(ion::Vector4((float)fbxBoneMatrix.Get(0, 0), (float)fbxBoneMatrix.Get(0, 1), (float)fbxBoneMatrix.Get(0, 2), (float)fbxBoneMatrix.Get(0, 3)));
					boneMatrix.SetAxisY(ion::Vector4((float)fbxBoneMatrix.Get(1, 0), (float)fbxBoneMatrix.Get(1, 1), (float)fbxBoneMatrix.Get(1, 2), (float)fbxBoneMatrix.Get(1, 3)));
					boneMatrix.SetAxisZ(ion::Vector4((float)fbxBoneMatrix.Get(2, 0), (float)fbxBoneMatrix.Get(2, 1), (float)fbxBoneMatrix.Get(2, 2), (float)fbxBoneMatrix.Get(2, 3)));
					boneMatrix.SetAxisW(ion::Vector4((float)fbxBoneMatrix.Get(3, 0), (float)fbxBoneMatrix.Get(3, 1), (float)fbxBoneMatrix.Get(3, 2), (float)fbxBoneMatrix.Get(3, 3)));
					boneMatrix.SetAxisW(scaleMat * boneMatrix.GetAxisW());
*/
					mat4x4* boneMatrix = &pose->m_bones[j];
					boneMatrix->xAxis = (vec4){ (float)fbxBoneMatrix.Get(0, 0), (float)fbxBoneMatrix.Get(0, 1), (float)fbxBoneMatrix.Get(0, 2), (float)fbxBoneMatrix.Get(0, 3) };
					boneMatrix->yAxis = (vec4){ (float)fbxBoneMatrix.Get(1, 0), (float)fbxBoneMatrix.Get(1, 1), (float)fbxBoneMatrix.Get(1, 2), (float)fbxBoneMatrix.Get(1, 3) };
					boneMatrix->zAxis = (vec4){ (float)fbxBoneMatrix.Get(2, 0), (float)fbxBoneMatrix.Get(2, 1), (float)fbxBoneMatrix.Get(2, 2), (float)fbxBoneMatrix.Get(2, 3) };
					boneMatrix->wAxis = vec4_transform(scaleMat, (vec4){ (float)fbxBoneMatrix.Get(3, 0), (float)fbxBoneMatrix.Get(3, 1), (float)fbxBoneMatrix.Get(3, 2), (float)fbxBoneMatrix.Get(3, 3) });

					const char* nodeName = lCluster->GetLink()->GetName();
					printf("Node: %s (%d)\n", nodeName, 0);

					if (m_firstFrame)
					{
						//					//Store Bone Matrix
						IonFbxBone newBone;
						newBone.m_name = nodeName;
						newBone.m_boneMatrix = *boneMatrix;
						_bones.push_back(newBone);
//						char text[256];
//						sprintf(text, "Bone %d (%s)", j, nodeName);
//						ion::PrintMatrix(text, boneMatrix);
					}
					pose->m_bones[j] = *boneMatrix;
				}
				static int s_numPoses = 0;
				printf("Break on pose %d\n", s_numPoses++);
            }
        }
	}
							
    void FbxConverter::AnimateSkeleton(float time)
    {
/*
		int poseIndex = GetPose((double)time);
		IonFbxPose* pose = m_pPoses + poseIndex;
		ion::Matrix44 *invBindPose = m_pMeshes->GetInverseBindPose();
        for (uint32_t j=0; j<pose->m_numBones; ++j)
        {            
            ion::Matrix44& boneMatrix = pose->m_bones[j];
            ion::Matrix44 &matrix = m_pMeshes->GetBone(j);
            matrix = boneMatrix * invBindPose[j];	//invBindMatrix;
        }
*/
    }	
	
//	FbxPose *FbxConverter::GetPose(float time)
	int FbxConverter::GetPose(double time)
	{
		float freq = 1.0f / m_frameRate;
		uint32_t index = (uint32_t)((time-m_startTime) / freq);
		printf("Time = %.3lf, Pose Index = %d\n", time, index);
		return index;
	}
	
	struct iPhoneTriangle
	{
		uint16_t m_verts[3];
	};
	
	void FbxConverter::Export(std::string filename, std::vector<IonFbxMesh*>& models)
	{
		printf("Exporting models to %s\n", filename.c_str());

		struct ModelHeader header;
		header.chunkId.fourCC = kFourCC_SCNE;
		header.chunkId.size = 0;
		header.versionMajor = kVersionMajor;
		header.versionMinor = kVersionMinor;
		 
//		int fd = _open(filename.c_str(), _O_WRONLY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE);
		FILE *fptr = fopen(filename.c_str(), "wb");

//		int bytesWritten = _write(fd, &header, sizeof(ModelHeader));
		fwrite(&header, 1, sizeof(struct ModelHeader), fptr);
		ExportTextures(fptr);
		ExportMaterials(fptr);
		ExportLights(fptr);
		ExportSkeleton(fptr, models[0]);
		ExportAnimation(fptr);
		ExportMeshes(fptr, models);
		ExportMeshInstances(fptr);
		fflush(fptr);
		fclose(fptr);		
	}
	
	void FbxConverter::ExportLights(FILE* fptr)
	{
		if (_lights.size() == 0)
		{
			return;
		}
		int nameSize = 0;
		for (size_t i = 0; i < _lights.size(); ++i)
		{
			uint32_t len = (uint32_t)_lights[i].m_name.length();
			int dataSize = (sizeof(uint32_t) + len) + 4 & ~(4 - 1);						//4 bytes length + string length + padding to 4 bytes
			nameSize += dataSize;
		}
		//		int numMaterials = (int)_materialHashes.size();
		int numLights = (int)_lights.size();
		int chunkSize = sizeof(LightChunk);
		int lightInfoSize = sizeof(LightInfo) * numLights;
		int totalSize = chunkSize + lightInfoSize + nameSize;

		uint8_t* data = (uint8_t*)malloc(totalSize);
		memset(data, 0, totalSize);
		//Create LightChunk
		LightChunk* chunk = (LightChunk*)data;
		chunk->chunkId.fourCC = kFourCC_LGHT;
		chunk->chunkId.size = totalSize;
		chunk->numLights = numLights;
		uint8_t* ptr = data + sizeof(LightChunk);

		//		printf("Exporting %d materials...\n", _materialHashes.size());
		printf("Exporting %zd lights...\n", _lights.size());
		for (size_t i = 0; i < _lights.size(); ++i)
		{
			IonFbxLight& thisLight = _lights[i];	//ion::MaterialManager::Get()->Find(*it);

			std::string& lightName = thisLight.m_name;
			printf("\t%s\n", lightName.c_str());
			uint32_t len = (uint32_t)lightName.length();
			uint32_t alignedLen = len + 4 & ~(4 - 1);						//4 bytes length + string length + padding to 4 bytes
			*(uint32_t*)ptr = len;
			ptr += sizeof(uint32_t);
			memcpy(ptr, lightName.c_str(), len);
			ptr += alignedLen;
			LightInfo* info = (LightInfo*)ptr;
			info->m_nameHash = hashString(lightName.c_str()); // ion::HashedString(lightName.c_str()).Get();
			memcpy(&info->m_transform, &thisLight.m_transform, sizeof(mat4x4));
			memcpy(&info->m_color, &thisLight.m_color, sizeof(vec4));
			ptr += sizeof(LightInfo);
		}
		AssertMsg((ptr - data) == totalSize, "Totals do not match");
		//		int bytesWritten = _write(fd, data, totalSize);
		int bytesWritten = (int)fwrite(data, 1, totalSize, fptr);
		return;
	}

	void FbxConverter::ExportTextures(FILE *fptr)
	{
//		if (_textureHashes.size() == 0) {
		if (_textures.size() == 0) {
			return;
		}
//		printf("Exporting %d textures...\n", _textureHashes.size());
		printf("Exporting %zd textures...\n", _textures.size());
		int totalDataSize = 0;
		std::set<IonFbxTexture*>::iterator it;
//		for(it = _textureHashes.begin(); it != _textureHashes.end(); ++it) {
		for(it = _textures.begin(); it != _textures.end(); ++it) {
			std::string textureName = (*it)->GetName();
//			bool bTextureFound = ion::TextureManager::Get()->FindName((*it), textureName);
//			AssertMsg(bTextureFound, "Texture not found.  This shouldn't happen");
			uint32_t len = (uint32_t)textureName.length();
			int dataSize = (sizeof(uint32_t) + len) + 4 & ~(4-1);						//4 bytes length + string length + padding to 4 bytes alignement
			totalDataSize += dataSize;
			printf("\t%s (0x%08x)\n", textureName.c_str(), (*it)->GetHash());
		}
		
		int chunkSize = sizeof(TextureChunk);
		int totalSize = chunkSize + totalDataSize;
		
		uint8_t *data = (uint8_t *)malloc(totalSize);
		//Create TextureChunk
		TextureChunk *chunk = (TextureChunk *)data;
		chunk->chunkId.fourCC = kFourCC_TEXT;
		chunk->chunkId.size = totalSize;
//		chunk->numTextures = _textureHashes.size();
		chunk->numTextures = static_cast<int>(_textures.size());
		
		uint8_t* ptr = data + sizeof(TextureChunk);
//		for(it = _textureHashes.begin(); it != _textureHashes.end(); ++it)
		for(it = _textures.begin(); it != _textures.end(); ++it)
		{
			std::string textureName = (*it)->GetName();
//			bool bTextureFound = ion::TextureManager::Get()->FindName((*it), textureName);
//			AssertMsg(bTextureFound, "Texture not found.  This shouldn't happen");
			uint32_t len = (uint32_t)textureName.length();
			uint32_t alignedLen = len + 4 & ~(4-1);						//4 bytes length + string length + padding to 4 bytes
//			uint32_t padBytes = alignedLen - len;
			*(uint32_t*)ptr = len;
			ptr+=sizeof(uint32_t);
			memcpy(ptr, textureName.c_str(), len);
			ptr += alignedLen;
		}
		AssertMsg((ptr-data) == totalSize, "Totals do not match");
//		int bytesWritten = _write(fd, data, totalSize);
		int bytesWritten = (int)fwrite(data, 1, totalSize, fptr);
		AssertMsg(bytesWritten == totalSize, "Totals do not match");
		return;
	}
	
	void FbxConverter::ExportMaterials(FILE *fptr)
	{
//		std::set<uint32_t>::iterator it;
		std::set<IonFbxMaterial*>::iterator it;
		int nameSize = 0;
//		for(it = _materialHashes.begin(); it != _materialHashes.end(); ++it)
		int pbrMatCount = 0;
		int matCount = 0;
		for(it = _materials.begin(); it != _materials.end(); ++it)
		{
			IonFbxMaterial* pMaterial = *it;
			uint32_t len = (uint32_t)pMaterial->GetName().length();
			int dataSize = (sizeof(uint32_t) + len) + 4 & ~(4-1);						//4 bytes length + string length + padding to 4 bytes
			nameSize += dataSize;
			if (pMaterial->m_flags & kMFPhysicallyBased)
				pbrMatCount++;
			else
				matCount++;
		}
//		int numMaterials = (int)_materialHashes.size();
		int numMaterials = (int)_materials.size();
		int chunkSize = sizeof(MaterialChunk);
		int materialInfoSize = (sizeof(MaterialInfoV3) * matCount) + (sizeof(PBRMaterialInfoV3) * pbrMatCount);
		int totalSize = chunkSize + materialInfoSize + nameSize;
		
		uint8_t *data = (uint8_t *)malloc(totalSize);
		memset(data, 0, totalSize);
		//Create MaterialChunk
		MaterialChunk *chunk = (MaterialChunk *)data;
		chunk->chunkId.fourCC = kFourCC_MATL;
		chunk->chunkId.size = totalSize;
		chunk->numMaterials = numMaterials;
		uint8_t* ptr = data + sizeof(MaterialChunk);
		
//		printf("Exporting %d materials...\n", _materialHashes.size());
		printf("Exporting %zd materials...\n", _materials.size());
//		int i = 0;
		int textureIndex = 0;
//		for(it = _materialHashes.begin(); it != _materialHashes.end(); ++it)
		for(it = _materials.begin(); it != _materials.end(); ++it)
		{
//			IonFbxMaterial* pMaterial = ion::MaterialManager::Get()->Find(*it);
			IonFbxMaterial* pMaterial = *it;	//ion::MaterialManager::Get()->Find(*it);
			
			std::string& materialName = pMaterial->GetName();
			printf("\t%s (0x%08llx) - Flags = 0x%08x\n", materialName.c_str(), (uint64_t)(*it), pMaterial->m_flags);
			uint32_t len = (uint32_t)materialName.length();
			uint32_t alignedLen = len + 4 & ~(4-1);						//4 bytes length + string length + padding to 4 bytes
			*(uint32_t*)ptr = len;
			ptr+=sizeof(uint32_t);
			memcpy(ptr, materialName.c_str(), len);
			ptr += alignedLen;
			if (pMaterial->m_flags & kMFPhysicallyBased)
			{
				PBRMaterialInfoV3* info = (PBRMaterialInfoV3*)ptr;
				memset(info, 0, sizeof(PBRMaterialInfoV3));
				info->flags = pMaterial->m_flags;
				if (info->flags & kMFDiffuseMap) {
					IonFbxTexture* tex = pMaterial->m_pDiffuseMap;
					info->diffuseMapHash = tex->GetHash();
				}
				else
				{
					uint32_t r = uint32_t(pMaterial->m_diffuse[0] * 255.0f);
					uint32_t g = uint32_t(pMaterial->m_diffuse[1] * 255.0f);
					uint32_t b = uint32_t(pMaterial->m_diffuse[2] * 255.0f);
					uint32_t a = uint32_t(pMaterial->m_alpha * 255.0f);
					info->diffuse = (r << 24) | (g << 16) | (b << 8) | a;
				}

				if (info->flags & kMFNormalMap) {
					IonFbxTexture* tex = pMaterial->m_pNormalMap;
					info->normalMapHash = tex->GetHash();
				}

				if (info->flags & kMFHeightMap)
				{
					IonFbxTexture* tex = pMaterial->m_pHeightMap;
					info->heightMapHash = tex->GetHash();
				}

				if (info->flags & kMFMetallicMap) {
					IonFbxTexture* tex = pMaterial->m_pMetallicMap;
					info->metallicMapHash = tex->GetHash();
				}
				else
				{
					info->metallic = 0.0f; //CLR - Handle non texture based metallic value.
				}

				if (info->flags & kMFRoughnessMap) {
					IonFbxTexture* tex = pMaterial->m_pRoughnessMap;
					info->roughnessMapHash = tex->GetHash();
				}
				else
				{
					info->roughness = 0.3f; //CLR - Handle non texture based roughness value.
				}

				if (info->flags & kMFEmissiveMap) {
					IonFbxTexture* tex = pMaterial->m_pEmissiveMap;
					info->emissiveMapHash = tex->GetHash();
				}
				else
				{
					info->emissive = 0; //CLR - Handle non texture based emissive value.
				}

				if (info->flags & kMFAmbientMap) {
					IonFbxTexture* tex = pMaterial->m_pAmbientMap;
					info->aoMapHash = tex->GetHash();
				}
				ptr += sizeof(PBRMaterialInfoV3);
			}
			else
			{
				MaterialInfoV3* info = (MaterialInfoV3*)ptr;
				info->diffuse[0] = pMaterial->m_diffuse[0];
				info->diffuse[1] = pMaterial->m_diffuse[1];
				info->diffuse[2] = pMaterial->m_diffuse[2];
				info->ambient[0] = pMaterial->m_ambient[0];
				info->ambient[1] = pMaterial->m_ambient[1];
				info->ambient[2] = pMaterial->m_ambient[2];
				info->specular[0] = pMaterial->m_specular[0];
				info->specular[1] = pMaterial->m_specular[1];
				info->specular[2] = pMaterial->m_specular[2];
				info->specular[3] = 0.5f; // pMaterial->m_specular[3];
				info->alpha = pMaterial->m_alpha;
				info->flags = pMaterial->m_flags;
				info->diffuseMapHash = -1;
				info->normalMapHash = -1;
				info->heightMapHash = -1;
				info->emissiveMapHash = -1;
				//			info->ambientMapHash = -1;
				if (info->flags & kMFDiffuseMap) {
					IonFbxTexture* tex = pMaterial->m_pDiffuseMap;
					IonFbxTexture* tempPtr = pMaterial->m_pDiffuseMap;
					tempPtr = 0;
					info->diffuseMapHash = tex->GetHash();
				}
				if (info->flags & kMFNormalMap) {
					IonFbxTexture* tex = pMaterial->m_pNormalMap;
					IonFbxTexture* tempPtr = pMaterial->m_pNormalMap;
					tempPtr = 0;
					info->normalMapHash = tex->GetHash();
				}
				if (info->flags & kMFHeightMap)
				{
					IonFbxTexture* tex = pMaterial->m_pHeightMap;
					//				IonFbxTexture* tempPtr = pMaterial->m_pNormalMap;
					//				tempPtr = 0;
					info->heightMapHash = tex->GetHash();
				}
				if (info->flags & kMFEmissiveMap) {
					IonFbxTexture* tex = pMaterial->m_pEmissiveMap;
					IonFbxTexture* tempPtr = pMaterial->m_pEmissiveMap;
					tempPtr = 0;
					info->emissiveMapHash = tex->GetHash();
				}
				ptr += sizeof(MaterialInfoV3);
			}
/*
			if (info->flags & 0x10) {
				IonFbxTexture* tex = pMaterial->m_pAmbientMap;
				IonFbxTexture* tempPtr = pMaterial->m_pAmbientMap;
				tempPtr = 0;
				info->ambientMapHash = tex->GetHash().Get();
			}
*/
//			i++;
//			ptr += sizeof(ion::MaterialInfo);
		}
		AssertMsg((ptr-data) == totalSize, "Totals do not match");		
//		int bytesWritten = _write(fd, data, totalSize);
		int bytesWritten = (int)fwrite(data, 1, totalSize, fptr);
		return;
	}

	void FbxConverter::ExportSkeleton(FILE *fptr, IonFbxMesh* mesh)
	{
		if (_bones.size() == 0) 
		{
			return;
		}
		printf("Exporting %zd bones...\n", _bones.size());
		int totalDataSize = _bones.size() * (sizeof(mat4x4) * 2);
		std::vector<IonFbxBone>::iterator it;

		for (it = _bones.begin(); it != _bones.end(); ++it) {
			std::string boneName = (*it).m_name;
			uint32_t len = (uint32_t)boneName.length();
			int dataSize = (sizeof(uint32_t) + len) + 4 & ~(4 - 1);						//4 bytes length + string length + padding to 4 bytes alignement
			dataSize += sizeof(mat4x4);
			totalDataSize += dataSize;
		}
		int chunkSize = sizeof(SkeletonChunk);
		int totalSize = chunkSize + totalDataSize;

		uint8_t *data = (uint8_t *)malloc(totalSize);
		//Create SkeletonChunk
		SkeletonChunk *chunk = (SkeletonChunk *)data;
		chunk->chunkId.fourCC = kFourCC_SKEL;
		chunk->chunkId.size = totalSize;
		chunk->numBones = static_cast<int>(_bones.size());

//		uint8_t* ptr = data + sizeof(ion::SkeletonChunk);
		uint8_t* ptr = data + sizeof(SkeletonChunk);
		//		for(it = _textureHashes.begin(); it != _textureHashes.end(); ++it)
		uint32_t index = 0;
		for (it = _bones.begin(); it != _bones.end(); ++it, ++index)
		{
			std::string boneName = (*it).m_name;
			mat4x4 boneMatrix = (*it).m_boneMatrix;
			const mat4x4 *inverseBindPose = mesh->GetInverseBindPose();
			mat4x4 inverseBindMatrix = inverseBindPose[index];
			//			bool bTextureFound = ion::TextureManager::Get()->FindName((*it), textureName);
			//			AssertMsg(bTextureFound, "Texture not found.  This shouldn't happen");
			uint32_t len = (uint32_t)boneName.length();
			uint32_t alignedLen = len + 4 & ~(4 - 1);						//4 bytes length + string length + padding to 4 bytes
			*(uint32_t*)ptr = len;
			ptr += sizeof(uint32_t);
			memcpy(ptr, boneName.c_str(), len);
			ptr += alignedLen;
			*(mat4x4*)ptr = boneMatrix;
			ptr += sizeof(mat4x4);
			*(mat4x4*)ptr = inverseBindMatrix;
			ptr += sizeof(mat4x4);
		}
		AssertMsg((ptr - data) == totalSize, "Totals do not match");
		//		int bytesWritten = _write(fd, data, totalSize);
		int bytesWritten = (int)fwrite(data, 1, totalSize, fptr);
		AssertMsg(bytesWritten == totalSize, "Totals do not match");
		return;
	}

	void FbxConverter::ExportAnimation(FILE* fptr)
	{
		int numTakes = m_numTakes;
		int totalFrames = 0;
		for (int i = 0; i < numTakes; ++i)
		{
			totalFrames += m_pTakes[i].numFrames;
		}
		if (_bones.size() == 0 || totalFrames == 1)
		{
			return;
		}

		int chunkSize = sizeof(AnimationChunk);
		int takeInfoSize = sizeof(TakeInfo) * numTakes;
		int numBones = (int)_bones.size();
		int poseSize = sizeof(mat4x4) * numBones * totalFrames; // m_numFrames;
		int totalSize = chunkSize + takeInfoSize + poseSize;
		uint8_t* data = (uint8_t*)malloc(totalSize);

		AnimationChunk* chunk = (AnimationChunk*)data;
		chunk->chunkId.fourCC = kFourCC_ANIM;
		chunk->chunkId.size = totalSize;
		chunk->numTakes = 1;

		TakeInfo* info = (TakeInfo*)((uint8_t*)data + sizeof(AnimationChunk));
		for (int i = 0; i < numTakes; ++i, ++info)
		{
			info->numFrames = m_pTakes[i].numFrames;
			info->startTime = 0.0331999995f;
			info->endTime = 1.0f;
			info->frameRate = m_frameRate;
		}
		uint8_t* ptr = (uint8_t*)info;
		for (uint32_t take = 0; take < m_numTakes; ++take)
		{
			for (uint32_t frame = 0; frame < m_pTakes[take].numFrames; frame++)
			{
				memcpy(ptr, m_pTakes[take].frames[frame].m_bones, sizeof(mat4x4) * m_pTakes[take].frames[frame].m_numBones);
				ptr += (sizeof(mat4x4) * m_pTakes[take].frames[frame].m_numBones);
			}
		}
		AssertMsg((ptr - data) == totalSize, "Totals do not match");
		//		int bytesWritten = _write(fd, data, totalSize);
		int bytesWritten = (int)fwrite(data, 1, totalSize, fptr);
		AssertMsg(bytesWritten == totalSize, "Totals do not match");
		return;
	}

#if 1	
	void FbxConverter::ExportMeshes(FILE *fptr, std::vector<IonFbxMesh*>& models)
	{
		static reng::VertexElement positionElement = {0, 3, kFloat, false, 0};
		static reng::VertexElement normalElement = {1, 3, kFloat, false, 12};
		static reng::VertexElement texCoordElement = {2, 2, kFloat, false, 24};
//		static ion::VertexElement texCoord2Element = {3, 2, kFloat, false, 0};
		
//		memset(&m_stream1ExportTemplate, 0, sizeof(ion::Import::VertexStream));
//		m_stream1ExportTemplate.addElement(0, 3, ion::kFloat, false, 0);
//		m_stream1ExportTemplate.addElement(1, 3, ion::kFloat, false, 12);
//		m_stream1ExportTemplate.addElement(2, 2, ion::kFloat, false, 24);
//		uint32_t stride1 = m_stream1ExportTemplate.m_stride;
		
//		memset(&m_stream2ExportTemplate, 0, sizeof(ion::CRVertexStream));
//		m_stream2ExportTemplate.AddElement(2, 2, ion::kFloat, false, 0);
//		uint32_t stride2 = m_stream2ExportTemplate.m_stride;
//		m_stream2ExportTemplate.m_bufferType = 1;	//0 for Static, 1 for Dynamic
		
		int meshDataSize = 0;
		for (int i = 0; i < models.size(); ++i) {
			meshDataSize = CalcExportMeshSizeRecursive(meshDataSize, models[i]);
		}
		
		int chunkSize = sizeof(MeshChunk);
		int totalSize = chunkSize + meshDataSize;
		uint8_t *data = (uint8_t *)malloc(totalSize);
		
		MeshChunk *meshChunk = (MeshChunk *)data;
		meshChunk->chunkId.fourCC = kFourCC_MESH;
		meshChunk->chunkId.size = totalSize;
		meshChunk->numMeshes = (uint32_t)models.size();
		uint8_t* ptr = data + sizeof(MeshChunk);
		printf("Exporting %d Meshes\n", (uint32_t)models.size());
		printf("%d Unique Meshes\n", (uint32_t)m_meshes.size());
		for (int i = 0; i < models.size(); ++i) {
			ptr = ExportMeshRecursive(ptr, models[i]);
		}
		int writtenSize = ptr - data;
		AssertMsg((writtenSize == totalSize), "Totals do not match");		
//		int bytesWritten = _write(fd, data, totalSize);
		int bytesWritten = (int)fwrite(data, 1, totalSize, fptr);
	}
	
	void FbxConverter::ExportMeshInstances(FILE *fptr)
	{
		if (m_meshInstances.size() == 0)
		{
			return;
		}
#if 1
		int nameSize = 0;
		for (size_t i = 0; i < m_meshInstances.size(); ++i)
		{
			uint32_t len = (uint32_t)m_meshInstances[i]->name.length();
			int dataSize = (sizeof(uint32_t) + len) + 4 & ~(4 - 1);						//4 bytes length + string length + padding to 4 bytes
			nameSize += dataSize;
		}
		//		int numMaterials = (int)_materialHashes.size();
		int numMeshInstances = (int)m_meshInstances.size();
		int chunkSize = sizeof(MeshInstanceChunk);
		int instanceInfoSize = sizeof(MeshInstanceInfo) * numMeshInstances;
		int totalSize = chunkSize + instanceInfoSize + nameSize;

		uint8_t* data = (uint8_t*)malloc(totalSize);
		memset(data, 0, totalSize);
		//Create MeshInstanceChunk
		MeshInstanceChunk* chunk = (MeshInstanceChunk*)data;
		chunk->chunkId.fourCC = kFourCC_INST;
		chunk->chunkId.size = totalSize;
		chunk->numMeshInstances = numMeshInstances;
		uint8_t* ptr = data + sizeof(MeshInstanceChunk);

		printf("Exporting %zd mesh instances...\n", m_meshInstances.size());
		for (size_t i = 0; i < m_meshInstances.size(); ++i)
		{
			IonFbxMeshInstance& thisInstance = *m_meshInstances[i];

			std::string& instanceName = thisInstance.name;
			printf("\t%s\n", instanceName.c_str());
			uint32_t len = (uint32_t)instanceName.length();
			uint32_t alignedLen = len + 4 & ~(4 - 1);						//4 bytes length + string length + padding to 4 bytes
			*(uint32_t*)ptr = len;
			ptr += sizeof(uint32_t);
			memcpy(ptr, instanceName.c_str(), len);
			ptr += alignedLen;
			MeshInstanceInfo* info = (MeshInstanceInfo*)ptr;
			info->meshHash = hashString(thisInstance.mesh->GetName());
			memcpy(&info->worldMatrix, &thisInstance.worldMatrix, sizeof(mat4x4));
			ptr += sizeof(MeshInstanceInfo);
		}
		AssertMsg((ptr - data) == totalSize, "Totals do not match");
		//		int bytesWritten = _write(fd, data, totalSize);
		int bytesWritten = (int)fwrite(data, 1, totalSize, fptr);
#endif
		return;
	}

	int FbxConverter::CalcExportMeshSizeRecursive(int currentSize, IonFbxMesh* model)
	{
		int size = currentSize;
//		uint32_t stride1 = m_stream1ExportTemplate.m_stride;
//		uint32_t stride2 = m_stream2ExportTemplate.m_stride;
		
		size += sizeof(MeshInfo);
		if (kVersionMajor > 0 || (kVersionMajor == 0 && kVersionMinor > 2))
		{
			size += sizeof(uint32_t);
		}

		for (uint32_t subIdx = 0; subIdx < model->GetNumSubMeshes(); ++subIdx) {
			size += sizeof(RenderableInfo);
			IonFbxSubMesh* pSubMesh = model->GetSubMesh(subIdx);
			VertexBuffer *vertexBuffer = pSubMesh->getVertexBuffer();
			uint32_t numStreams = vertexBuffer->getNumStreams();
			reng::Import::VertexStream *stream = vertexBuffer->getVertexStreams();
			uint32_t stream1Size = stream->m_stride * vertexBuffer->getNumVertices();	// pSubMesh->GetNumVertices();
//			int stream2Size = stride2 * pRenderable->GetNumVertices();
			uint32_t verticesSize = sizeof(reng::Import::VertexBuffer) + stream1Size;	//+ stream2Size;
			size += verticesSize;
			reng::Import::IndexBuffer* indexBuffer = pSubMesh->GetIndexBuffer();
			if (indexBuffer)
			{
				int indicesSize = 0;
				switch (indexBuffer->GetType())
				{
				case reng::Import::k32Bit:
						indicesSize = indexBuffer->GetNumIndices() * sizeof(uint32_t);
						break;
				case reng::Import::k16Bit:
						indicesSize = indexBuffer->GetNumIndices() * sizeof(uint16_t);
						break;
				}
				size += indicesSize;
			}
		}
		for (uint32_t child = 0; child < model->GetNumChildren(); ++child) {
			IonFbxMesh* pChild = model->GetChild(child);
			size = CalcExportMeshSizeRecursive(size, pChild);
		}
		return size;
	}
	
	uint8_t* FbxConverter::ExportMeshRecursive(uint8_t *ptr, IonFbxMesh *model)
	{
#if 1
//		uint32_t stride1 = m_stream1ExportTemplate.m_stride;
//		uint32_t stride2 = m_stream2ExportTemplate.m_stride;

		MeshInfo *info = (MeshInfo *)ptr;
		memcpy(&info->worldMatrix, &model->GetWorldMatrix(), sizeof(mat4x4));
		info->numRenderables = model->GetNumSubMeshes();
		info->numChildren = model->GetNumChildren();
		AABB aabb = model->GetAABB();
		info->aabbMinX = aabb.aabbMin.x;
		info->aabbMinY = aabb.aabbMin.y;
		info->aabbMinZ = aabb.aabbMin.z;
		info->aabbMaxX = aabb.aabbMax.x;
		info->aabbMaxY = aabb.aabbMax.y;
		info->aabbMaxZ = aabb.aabbMax.z;
		ptr += sizeof(MeshInfo);
		if (kVersionMajor > 0 || (kVersionMajor == 0 && kVersionMinor > 2))
		{
			uint32_t *nameHash = (uint32_t *)ptr;
			*nameHash = hashString(model->GetName());
			ptr += sizeof(uint32_t);
		}
		RenderableInfo* rendInfo = (RenderableInfo*)ptr;
		uint8_t *meshData = (uint8_t *)((uint8_t *)rendInfo + sizeof(RenderableInfo) * info->numRenderables);
		for (uint32_t subMeshIdx = 0; subMeshIdx < model->GetNumSubMeshes(); ++subMeshIdx)
		{
			IonFbxSubMesh* pSubMesh = model->GetSubMesh(subMeshIdx);
			VertexBuffer *vertexBuffer = pSubMesh->getVertexBuffer();
			reng::Import::VertexStream *vertexStreams = vertexBuffer->getVertexStreams();
			int stream1Size = vertexStreams[0].m_stride * pSubMesh->GetNumVertices();
//			int stream2Size = stride2 * pMaterialGroup->GetNumVertices();
			int verticesSize = sizeof(reng::Import::VertexBuffer) + stream1Size;	//+ stream2Size;
//			int indicesSize = sizeof(uint32_t) * pRenderable->GetNumIndices();
			int indicesSize = 0;
			reng::Import::IndexBuffer *pIndexBuffer = pSubMesh->GetIndexBuffer();
			if (pIndexBuffer)
			{
				switch (pIndexBuffer->GetType())
				{
				case reng::Import::k32Bit:
						indicesSize = pIndexBuffer->GetNumIndices() * sizeof(uint32_t);
						break;
				case reng::Import::k16Bit:
						indicesSize = pIndexBuffer->GetNumIndices() * sizeof(uint16_t);
						break;
				}
			}
#if 1		
			reng::Import::VertexBuffer *dstVertexBuffer = (reng::Import::VertexBuffer *)meshData;
///			vertexBuffer->Init(1);
			dstVertexBuffer->m_numStreams = vertexBuffer->getNumStreams();
			size_t streamSize = sizeof(reng::Import::VertexStream);
			memcpy(&dstVertexBuffer->m_streams[0], vertexStreams, streamSize);
			//			memcpy(&vertexBuffer->m_streams[1], &stream2, streamSize);
			
			float *vertexData1 = (float *)((uint8_t *)dstVertexBuffer + sizeof(reng::Import::VertexBuffer));
			dstVertexBuffer->m_streams[0].m_dataOffset = (uint32_t)((uint8_t *)vertexData1 - (uint8_t *)info);
			/*
			 float *vertexData2 = (float *)((uint8_t *)vertexData1 + stride1 * pMaterialGroup->GetNumVertices());
			 vertexBuffer->m_streams[1].m_data = (void *)((uint8_t *)vertexData2 - (uint8_t *)data);
			 */
			//iPhoneTriangle *tris = (iPhoneTriangle *)((uint8_t*)vertexBuffer + verticesSize);
			uint32_t* indices = (uint32_t*)((uint8_t*)dstVertexBuffer + verticesSize);
			
			rendInfo[subMeshIdx].numVertices = pSubMesh->GetNumVertices();
			rendInfo[subMeshIdx].numIndices = 0;
			if (pIndexBuffer)
			{
				rendInfo[subMeshIdx].numIndices = pIndexBuffer->GetNumIndices();
			}
			rendInfo[subMeshIdx].materialHash = pSubMesh->GetMaterial()->GetHash();
			AABB aabb = pSubMesh->GetAABB();
			rendInfo[subMeshIdx].aabbMinX = aabb.aabbMin.x;
			rendInfo[subMeshIdx].aabbMinY = aabb.aabbMin.y;
			rendInfo[subMeshIdx].aabbMinZ = aabb.aabbMin.z;
			rendInfo[subMeshIdx].aabbMaxX = aabb.aabbMax.x;
			rendInfo[subMeshIdx].aabbMaxY = aabb.aabbMax.y;
			rendInfo[subMeshIdx].aabbMaxZ = aabb.aabbMax.z;
			rendInfo[subMeshIdx].verticesOffset = (uint32_t)((uint8_t *)vertexBuffer - (uint8_t *)info);
			rendInfo[subMeshIdx].indicesOffset = (uint32_t)((uint8_t *)indices - (uint8_t *)info);

//Copy vertices
//			VertexBuffer* vertexBuffer = pSubMesh->getVertexBuffer();
//			ion::Import::VertexStream* vertexStreams = vertexBuffer->getVertexStreams();
			memcpy(vertexData1, vertexBuffer->getData(), vertexStreams[0].m_stride * pSubMesh->GetNumVertices());
			
//Copy indices
			pIndexBuffer = pSubMesh->GetIndexBuffer();
			if (pIndexBuffer)
			{
				void* dataPtr = pIndexBuffer->GetData();
				AssertMsg(dataPtr != 0, "IndexBuffer: CPU Data copy not available!");
				memcpy(indices, dataPtr, indicesSize);
			}
			meshData += verticesSize + indicesSize;
#endif
		}
		ptr = (uint8_t*)meshData;
		for (uint32_t child = 0; child < model->GetNumChildren(); ++child) {
			IonFbxMesh* pChild = model->GetChild(child);
			ptr = ExportMeshRecursive(ptr, pChild);
		}
#endif
		return ptr;
	}
#endif
/*
	void IonFbxImporter::Export(std::string filename)
	{
		printf("Exporting model to %s\n", filename.c_str());
		
		ModelHeader header;
		memcpy(header.chunkId.tag, "MODL", 4);
		header.chunkId.size = 0;
		
		int fd = open(filename.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IROTH);
		write(fd, &header, sizeof(ModelHeader));		
		ExportTextures(fd);
		ExportMaterials(fd);
		ExportMeshes(fd);
#if 0		
		std::map<std::string, ObjMaterialGroup*>::iterator it;	
		
		int numMaterials = m_materialGroups.size();
		int numTextures = 0;
		for(it = m_materialGroups.begin(); it != m_materialGroups.end(); ++it)
		{
			ObjMaterialGroup* pMaterialGroup = (*it).second;			
			ObjMaterial* pMaterial = pMaterialGroup->GetMaterial();
			ion::TexturePtr texturePtr = pMaterial->GetDiffuseMap();
			if (texturePtr)
				numTextures++;
		}
		printf("%d Materials, %d Textures\n", numMaterials, numTextures);
		
		for(it = m_materialGroups.begin(); it != m_materialGroups.end(); ++it)
		{
			ObjMaterialGroup* pMaterialGroup = (*it).second;			
			
			ion::CRVertexStream stream1;
			memset(&stream1, 0, sizeof(ion::CRVertexStream));
			stream1.AddElement(0, 3, GL_FLOAT, GL_FALSE, 0);
			stream1.AddElement(1, 3, GL_FLOAT, GL_FALSE, 12);
			//	VertexStreamAddElement(&stream1, 2, 2, GL_FLOAT, GL_FALSE, 24);
			uint32_t stride1 = stream1.m_stride;
			
			
			ion::CRVertexStream stream2;
			memset(&stream2, 0, sizeof(ion::CRVertexStream));
			stream2.AddElement(2, 2, GL_FLOAT, GL_FALSE, 0);
			uint32_t stride2 = stream2.m_stride;
			stream2.m_bufferType = 1;	//0 for Static, 1 for Dynamic
			
			ModelHeader header;
			memcpy(header.chunkId.tag, "MODL", 4);
			header.chunkId.size = 0;
			header.numMeshes = 1;
			
			int meshChunkSize = sizeof(MeshChunk);
			int verticesSize = sizeof(ion::CRVertexBuffer) + stride1 * pMaterialGroup->GetNumVertices();
			verticesSize += stride2 * pMaterialGroup->GetNumVertices();
			int trisSize = sizeof(ionp::ObjFatFace) * pMaterialGroup->GetNumFaces();
			int totalSize = meshChunkSize + verticesSize + trisSize;
			
			void *data = malloc(totalSize);
			
			MeshChunk *meshChunk = (MeshChunk *)data;
			ion::CRVertexBuffer *vertexBuffer = (ion::CRVertexBuffer *)((uint8_t *)data + meshChunkSize);
			vertexBuffer->Init(2);
			memcpy(&vertexBuffer->m_streams[0], &stream1, sizeof(ion::CRVertexStream));
			memcpy(&vertexBuffer->m_streams[1], &stream2, sizeof(ion::CRVertexStream));
			
			float *vertexData1 = (float *)((uint8_t *)vertexBuffer + sizeof(ion::CRVertexBuffer));
			vertexBuffer->m_streams[0].m_data = (void *)((uint8_t *)vertexData1 - (uint8_t *)data);
			float *vertexData2 = (float *)((uint8_t *)vertexData1 + stride1 * pMaterialGroup->GetNumVertices());
			vertexBuffer->m_streams[1].m_data = (void *)((uint8_t *)vertexData2 - (uint8_t *)data);
			
			iPhoneTriangle *tris = (iPhoneTriangle *)((uint8_t*)vertexBuffer + verticesSize);
			
			memcpy(meshChunk->chunkId.tag, "MESH", 4);
			meshChunk->chunkId.size = totalSize;
			meshChunk->numVertices = pMaterialGroup->GetNumVertices();
			meshChunk->numTris = pMaterialGroup->GetNumFaces();
			meshChunk->verticesOffset = (uint8_t *)vertexBuffer - (uint8_t *)data;
			meshChunk->trisOffset = (uint8_t *)tris - (uint8_t *)data;
			float *ptr = vertexData1;
			
			FatVert *pVertices = pMaterialGroup->GetVertices();
			for (int j = 0; j < pMaterialGroup->GetNumVertices(); j++)
			{
				float scale = 1.0f; //0.7f;
				*(ptr++) = pVertices[j].vertex.x * scale;
				*(ptr++) = pVertices[j].vertex.y * scale;
				*(ptr++) = pVertices[j].vertex.z * scale;
				*(ptr++) = pVertices[j].normal.x;
				*(ptr++) = pVertices[j].normal.y;
				*(ptr++) = pVertices[j].normal.z;
				//		*(ptr++) = pMesh->m_uvs[j].u;
				//		*(ptr++) = pMesh->m_uvs[j].v;			
			}
			
			ptr = vertexData2;
			
			for (int j = 0; j < pMaterialGroup->GetNumVertices(); j++)
			{
				*(ptr++) = pVertices[j].texCoord.u;
				*(ptr++) = pVertices[j].texCoord.v;			
			}
			
			ObjFatFace *pTris = pMaterialGroup->GetFaces();
			for (int j = 0; j < pMaterialGroup->GetNumFaces(); j++)
			{
				tris[j].m_verts[0] = pTris[j].m_verts[0];
				tris[j].m_verts[1] = pTris[j].m_verts[1];
				tris[j].m_verts[2] = pTris[j].m_verts[2];
			}
			write(fd, data, totalSize);
			close(fd);
			
		}
#endif
		close(fd);
		
	}	
	
	void IonFbxImporter::ExportTextures(int fd)
	{
		
	}
	
	void IonFbxImporter::ExportMaterials(int fd)
	{
		
	}
	
	void IonFbxImporter::ExportMeshes(int fd)
	{
		
	}
*/

	IonFbxTexture::IonFbxTexture(std::string name)
	{
		m_name = name;
		m_hash = hashString(name.c_str());
	}
	
	IonFbxPose::IonFbxPose()
	{
		m_numBones = 0;
		m_bones	   = 0;
	}
	
	IonFbxPose::~IonFbxPose()
	{
		m_numBones = 0;
		delete [] m_bones;
		m_bones = 0;
	}
	
#if 0    
	FbxFatVert::FbxFatVert()
	{
	}
    
	FbxFatVert::FbxFatVert( Vertex& v, TexCoord& vt )
	{
		vertex.x = v.x;
		vertex.y = v.y;
		vertex.z =	v.z;
		texCoord0.u = vt.u;
		texCoord0.v = vt.v;
	}
#endif
/*
#pragma mark IonFbxMateriaal
	
	IonFbxMaterial::IonFbxMaterial()
	{
		
	}
	
	IonFbxMaterial::~IonFbxMaterial()
	{
		
	}
*/


#if 1
	IonFbxSubMesh::IonFbxSubMesh()
	{
		//		m_flags = 0;
		m_vertexBuffer = new VertexBuffer();	// VertexBuffer::Create();
		m_indexBuffer = 0;
		m_numVertices = 0;
	}
	
	IonFbxSubMesh::~IonFbxSubMesh()
	{
		//CLR: We should probably delete our vertex buffer here too.
//		if (m_indexBuffer) {
//			m_indexBuffer->Destroy();
//		}
	}
#endif
    
    IonFbxMesh::IonFbxMesh()
    {
		m_numBones = 0;
		m_numChildren = 0;
		m_numSubMeshes = 0;
		m_pChildren = 0;
    }
    
    IonFbxMesh::~IonFbxMesh()
    {
        
    }
    
	void IonFbxMesh::CreateChildren(uint32_t numChildren)
	{
		m_numChildren = numChildren;
		m_pChildren = new IonFbxMesh [numChildren];
	}
	
    void IonFbxMesh::ConvertMesh(const char* name, FbxMatrix& pGlobalPosition, FbxMesh* pMesh)
    {
        FbxMatrix nodeTransformMatrix = pGlobalPosition; 
		const char *meshName = pMesh->GetName();
		if (strstr(name, "ScoreSurround") != nullptr)
		{
			printf("Node Name: %s -> pMesh = 0x%016x\n", name, (uint64_t)pMesh);
		}
        mat4x4 nodeMatrix;
		nodeMatrix.xAxis = (vec4){ (float)nodeTransformMatrix.Get(0, 0), (float)nodeTransformMatrix.Get(0, 1), (float)nodeTransformMatrix.Get(0, 2), (float)nodeTransformMatrix.Get(0, 3) };
		nodeMatrix.yAxis = (vec4){ (float)nodeTransformMatrix.Get(1, 0), (float)nodeTransformMatrix.Get(1, 1), (float)nodeTransformMatrix.Get(1, 2), (float)nodeTransformMatrix.Get(1, 3) };
		nodeMatrix.zAxis = (vec4){ (float)nodeTransformMatrix.Get(2, 0), (float)nodeTransformMatrix.Get(2, 1), (float)nodeTransformMatrix.Get(2, 2), (float)nodeTransformMatrix.Get(2, 3) };
		nodeMatrix.wAxis = (vec4){ (float)nodeTransformMatrix.Get(3, 0), (float)nodeTransformMatrix.Get(3, 1), (float)nodeTransformMatrix.Get(3, 2), (float)nodeTransformMatrix.Get(3, 3) };

		mat4x4 scaleMat = mat4x4_scale(0.01f);
		
		m_name = std::string(name);
		uint32_t hash = hashString(m_name.c_str());
		printf("\tNode: %s (0x%08x)\n", m_name.c_str(), hash);
//		Mat44 myMat = scaleMat * nodeMatrix;
//		myMat.SetTranslate(Point3(-1.0f, 0.0f, 0.0f));
		m_worldMatrix.xAxis = nodeMatrix.xAxis;
		m_worldMatrix.yAxis = nodeMatrix.yAxis;
		m_worldMatrix.zAxis = nodeMatrix.zAxis;
		m_worldMatrix.wAxis = vec4_transform(scaleMat, nodeMatrix.wAxis);
		int count = pMesh->GetDeformerCount(FbxDeformer::eVertexCache);
	
        bool bSkinned = pMesh->GetDeformerCount(FbxDeformer::eSkin) > 0;

        // All the links must have the same link mode.

		FbxCluster::ELinkMode lClusterMode;
		if (bSkinned)
		{
			lClusterMode = ((FbxSkin*)pMesh->GetDeformer(0, FbxDeformer::eSkin))->GetCluster(0)->GetLinkMode();			
		}

		uint32_t numMaterials = pMesh->GetNode()->GetMaterialCount();	// <FbxSurfaceMaterial>();
		IonFbxMaterial** materials = new IonFbxMaterial*[numMaterials];

		ConvertMeshMaterials(pMesh, numMaterials, materials, bSkinned);

		m_numMaterials = numMaterials;
		m_materials = materials;

		int lClusterCount=0;
	
        int lVertexCount = pMesh->GetControlPointsCount();
        int vertexCount = lVertexCount;
        
		//Allocate Temporary Arrays for vertex processing        
        Vertex4Bone *srcVertices = new Vertex4Bone [vertexCount];                                //Vertices as stored in the FBX file.  These need to be remapped to a unique vertex array.
        memset( srcVertices, 0, sizeof(Vertex4Bone)*vertexCount);
        int *bonesPerVertex = new int [vertexCount];                                //Temporary count for the number of bones in each vertex.  Todo: Move this value into (Vertex4Bone).w
        memset( bonesPerVertex, 0, sizeof(int)*vertexCount);
		int maxBonesPerVertex = 0;
        //Get temporary pointer to Mesh Control Points (vertices in FBX speak)
        FbxVector4 *controlPoints = pMesh->GetControlPoints();

		//CLR: Add first point to boundingBox;
		
		vec4 resetPoint = vec4_transform(scaleMat, (vec4) { (float)controlPoints[0][0], (float)controlPoints[0][1], (float)controlPoints[0][2], 1.0f });
		m_aabb.Reset(resetPoint);

		if (bSkinned)
		{
			int lSkinCount=pMesh->GetDeformerCount(FbxDeformer::eSkin);
			for(int i=0; i<lSkinCount; ++i)
			{
                lClusterCount += ((FbxSkin *)(pMesh->GetDeformer(i, FbxDeformer::eSkin)))->GetClusterCount();
				//            m_matrixPalette = new KFbxXMatrix [lClusterCount];
				m_numBones = lClusterCount;
				m_inverseBindPose = new mat4x4 [lClusterCount];
				m_bonePalette = new mat4x4 [lClusterCount];
				for (int j=0; j<lClusterCount; ++j)
				{					
					FbxCluster* lCluster =((FbxSkin *)pMesh->GetDeformer(i, FbxDeformer::eSkin))->GetCluster(j);
					if (!lCluster->GetLink())
					{
						//                    printf("skipping cluster %d out of %d\n", j, lClusterCount);
						continue;
					}
					
					//Get Bind Pose Matrix
					FbxAMatrix fbxBindMatrix;
					lCluster->GetTransformLinkMatrix(fbxBindMatrix);
					//Convert to ion friendly format            
					mat4x4 bindMatrix;
					bindMatrix.xAxis = (vec4){ (float)fbxBindMatrix.Get(0, 0), (float)fbxBindMatrix.Get(0, 1), (float)fbxBindMatrix.Get(0, 2), (float)fbxBindMatrix.Get(0, 3) };
					bindMatrix.yAxis = (vec4){ (float)fbxBindMatrix.Get(1, 0), (float)fbxBindMatrix.Get(1, 1), (float)fbxBindMatrix.Get(1, 2), (float)fbxBindMatrix.Get(1, 3) };
					bindMatrix.zAxis = (vec4){ (float)fbxBindMatrix.Get(2, 0), (float)fbxBindMatrix.Get(2, 1), (float)fbxBindMatrix.Get(2, 2), (float)fbxBindMatrix.Get(2, 3) };
					bindMatrix.wAxis = vec4_transform(scaleMat, (vec4) { (float)fbxBindMatrix.Get(3, 0), (float)fbxBindMatrix.Get(3, 1), (float)fbxBindMatrix.Get(3, 2), (float)fbxBindMatrix.Get(3, 3) });
					//Calc Inverse Bind Pose Matrix
					//				Mat44 invBindMatrix = OrthoInverse(nodeMatrix * bindMatrix);
					m_inverseBindPose[j] = mat4x4_orthoInverse(bindMatrix); ///*ion::Matrix44(ion::kIdentity); // OrthoInverse(nodeMatrix * bindMatrix);*/
					
					int lVertexIndexCount = lCluster->GetControlPointIndicesCount();
					
					for (int k = 0; k < lVertexIndexCount; ++k) 
					{            
						int lIndex = lCluster->GetControlPointIndices()[k];
						double lWeight = lCluster->GetControlPointWeights()[k];
						
						if (lClusterMode == FbxCluster::eAdditive)
						{   
							printf("Additive Cluster Deformation not supported\n");
							exit(1);
						}
						else // lLinkMode == KFbxLink::eNORMALIZE || lLinkMode == KFbxLink::eTOTAL1
						{
							if (bonesPerVertex[lIndex] < 4) 
							{                                
								int boneIndex = bonesPerVertex[lIndex]++;
//								srcVertices[lIndex].srcVertex = ion::Vector4((float)controlPoints[lIndex][0], (float)controlPoints[lIndex][1], (float)controlPoints[lIndex][2], 1.0f);  //controlPoints[lIndex];
								srcVertices[lIndex].srcVertex = vec4_transform(scaleMat, (vec4) { (float)controlPoints[lIndex][0], (float)controlPoints[lIndex][1], (float)controlPoints[lIndex][2] });  //controlPoints[lIndex];
								srcVertices[lIndex].boneIndices[boneIndex] = j;
								srcVertices[lIndex].boneWeights[boneIndex] = (float)lWeight;
							}
							else
							{
								bonesPerVertex[lIndex]++;
							}
							if (bonesPerVertex[lIndex] > maxBonesPerVertex)
								maxBonesPerVertex = bonesPerVertex[lIndex];

						}  
						//CLR: Add subsequant points to boundingBox;
						m_aabb.Add(srcVertices[lIndex].srcVertex);
					}
				}
			}
			
			//Normalise Weights
			for (int i = 0; i < lVertexCount; i++) 
			{
				Vertex4Bone &vertex = srcVertices[i];
				float weightSumExceptSmallest = 0.0;
				for (int j = 0; j < bonesPerVertex[i] - 1; j++)
				{
					if (vertex.boneWeights[j] < vertex.boneWeights[j + 1])
					{
						float tempWeight = vertex.boneWeights[j + 1];
						int32_t tempIndex = vertex.boneIndices[j + 1];
						vertex.boneWeights[j + 1] = vertex.boneWeights[j];
						vertex.boneIndices[j + 1] = vertex.boneIndices[j];
						vertex.boneWeights[j] = tempWeight;
						vertex.boneIndices[j] = tempIndex;
					}
					weightSumExceptSmallest += vertex.boneWeights[j];
				}
// Ensure bone weights sum to 1.0.
				vertex.boneWeights[bonesPerVertex[i] - 1] = 1.0f - weightSumExceptSmallest;
				float totalWeight = 0.0f;
				for (int j = 0; j < bonesPerVertex[i]; j++)
				{
					float weight = vertex.boneWeights[j];
					totalWeight += weight;
				}       
				AssertMsg((totalWeight == 1.0f), "Weights still don't sum to 1.0\n");
/*
				float checkWeight = 0.0f;
				for (int j = 0; j < bonesPerVertex[i]-1; ++j)
					checkWeight += vertex.boneWeights[j];
//				if (checkWeight != 1.0f)
//					ion::print("Total Weight does equal 1.0\n");
				vertex.boneWeights[bonesPerVertex[i] - 1] = 1.0f - checkWeight;
*/
			}
			delete [] bonesPerVertex;			
		}
		else
		{
			for (int k = 0; k < lVertexCount; ++k) 
			{            
				srcVertices[k].srcVertex = vec4_transform(scaleMat, (vec4) { (float)controlPoints[k][0], (float)controlPoints[k][1], (float)controlPoints[k][2] });  //controlPoints[lIndex];
				for (int i = 0; i < 4; i++)
				{
					srcVertices[k].boneIndices[i] = -1;
					srcVertices[k].boneWeights[i] = 0.0f;					
				}
				//CLR: Add subsequant points to boundingBox;
				m_aabb.Add(srcVertices[k].srcVertex);
			}
		};
		
		printf("\t\tModel AABB:\t");
		m_aabb.Print();
 		int polyCount = pMesh->GetPolygonCount();

		static reng::Import::VertexElement positionElement = {0, 3, kFloat, false, 0};
		static reng::Import::VertexElement normalElement = {1, 4, kInt2_10_10_10_Rev, true, 12};
		static reng::Import::VertexElement tangentElement = { 2, 4, kInt2_10_10_10_Rev, true, 16 };
		static reng::Import::VertexElement binormalElement = { 3, 4, kInt2_10_10_10_Rev, true, 20 };
		static reng::Import::VertexElement texCoordElement = { 4, 2, kFloat, false, 24 };
		static reng::Import::VertexElement boneIndexElement = { 5, 4, kSignedInt, false, 32 };
		static reng::Import::VertexElement boneWeightElement = { 6, 4, kFloat, false, 48 };

#if 1
		m_numSubMeshes = numMaterials;
		m_subMeshes = new IonFbxSubMesh [m_numSubMeshes];
		for (uint32_t matId = 0; matId < numMaterials; ++matId)
		{	
			//Find Unique Vertices
			//Todo: This needs finishing off ASAP.
			UniqueVertexElements uniqueVertexElements;
			std::map<uint64_t, FbxFatVert> uniqueVerts;
			uint32_t triangleCount = GenerateFatVerts( pMesh, srcVertices, matId, uniqueVertexElements, uniqueVerts);
			bool bBumpMapped = uniqueVertexElements.uniqueTangents.size() > 0;

			// Iterate over unique vertex map creating unqiue vertex array and remapping table.		
			std::map<uint64_t, uint32_t> indexRemappingTable;
			std::size_t numUniqueVertices = uniqueVerts.size();
			vertexCount = (int)numUniqueVertices;
			Vertex* subMeshSrcVertices = new Vertex [numUniqueVertices];
			Vertex* subMeshNormals = new Vertex [numUniqueVertices];
			TexCoord* subMeshUVs = new TexCoord [numUniqueVertices];
			TexCoord* subMeshUV2s = new TexCoord [numUniqueVertices];
			Vertex* subMeshTangents = nullptr;
			Vertex* subMeshBinormals = nullptr;
			if (bBumpMapped)
			{
				subMeshTangents = new Vertex[numUniqueVertices];
				subMeshBinormals = new Vertex[numUniqueVertices];
			}
			Vector4i* subMeshBoneIndices = nullptr;
			vec4*  subMeshBoneWeights = nullptr;
			if (bSkinned)
			{
				subMeshBoneIndices = new Vector4i [numUniqueVertices];
				subMeshBoneWeights = new vec4[numUniqueVertices];
			}

			FbxStaticVertex* pStaticVertexData = new FbxStaticVertex[numUniqueVertices];
			int index = 0;
			for (auto fatMapIt = uniqueVerts.begin(); fatMapIt != uniqueVerts.end(); ++fatMapIt)
			{
				uint64_t vertexHash = (*fatMapIt).first;							//Get Vertex Hash
				FbxFatVert fatVert = (*fatMapIt).second;					//Get Vertex
				indexRemappingTable[vertexHash] = index;					//Add index to remapping table
				//Build static and dynamic unique vertex arrays.
#if 0				
				ion::Point xVertex = ion::Point(fatVert.vertex.x, fatVert.vertex.y, fatVert.vertex.z);
				if (index == 0)
				{
					m_subMeshes[matId].GetAABB().Reset(xVertex);
				}
				else
				{
					m_subMeshes[matId].GetAABB().Add(xVertex);
				}
				subMeshSrcVertices[index].x = xVertex.X();
				subMeshSrcVertices[index].y = xVertex.Y();
				subMeshSrcVertices[index].z = xVertex.Z();
				if (bSkinned)
				{
					subMeshBoneWeights[index] = ion::Vector4(fatVert.weights[0], fatVert.weights[1], fatVert.weights[2], fatVert.weights[3]);
					subMeshBoneIndices[index].x = fatVert.bones[0];
					subMeshBoneIndices[index].y = fatVert.bones[1];
					subMeshBoneIndices[index].z = fatVert.bones[2];
					subMeshBoneIndices[index].w = fatVert.bones[3];
				}
				pStaticVertexData[index].colour[0] = 1.0f;
				pStaticVertexData[index].colour[1] = 1.0f;
				pStaticVertexData[index].colour[2] = 1.0f;
				pStaticVertexData[index].uv[0] = fatVert.texCoord0.u;
				pStaticVertexData[index].uv[1] = fatVert.texCoord0.v;
				subMeshNormals[index].x = fatVert.normal.x;
				subMeshNormals[index].y = fatVert.normal.y;
				subMeshNormals[index].z = fatVert.normal.z;
				subMeshUVs[index].u = fatVert.texCoord0.u;
				subMeshUVs[index].v = fatVert.texCoord0.v;
				subMeshUV2s[index].u = fatVert.texCoord1.u;
				subMeshUV2s[index].v = fatVert.texCoord1.v;
#else
				vec4 xVertex = srcVertices[fatVert.uniqueVertexIndex].srcVertex;	// ion::Point(x, y, z);
				if (index == 0)
					m_subMeshes[matId].GetAABB().Reset(xVertex);
				else
					m_subMeshes[matId].GetAABB().Add(xVertex);

				subMeshSrcVertices[index].x = xVertex.x;
				subMeshSrcVertices[index].y = xVertex.y;
				subMeshSrcVertices[index].z = xVertex.z;
				if (bSkinned)
				{
					subMeshBoneWeights[index] = (vec4){ srcVertices[fatVert.uniqueVertexIndex].boneWeights[0], srcVertices[fatVert.uniqueVertexIndex].boneWeights[1], srcVertices[fatVert.uniqueVertexIndex].boneWeights[2], srcVertices[fatVert.uniqueVertexIndex].boneWeights[3] };
					subMeshBoneIndices[index].x = srcVertices[fatVert.uniqueVertexIndex].boneIndices[0];
					subMeshBoneIndices[index].y = srcVertices[fatVert.uniqueVertexIndex].boneIndices[1];
					subMeshBoneIndices[index].z = srcVertices[fatVert.uniqueVertexIndex].boneIndices[2];
					subMeshBoneIndices[index].w = srcVertices[fatVert.uniqueVertexIndex].boneIndices[3];
				}
				pStaticVertexData[index].color = Color(1.0f, 1.0f, 1.0f);
				pStaticVertexData[index].uv = uniqueVertexElements.uniqueUVs.getElement(fatVert.uniqueUvIndices[0]);
				subMeshNormals[index] = uniqueVertexElements.uniqueNormals.getElement(fatVert.uniqueNormalIndex);
				subMeshUVs[index] = uniqueVertexElements.uniqueUVs.getElement(fatVert.uniqueUvIndices[0]);
				if (bBumpMapped)
				{
					subMeshTangents[index] = uniqueVertexElements.uniqueTangents.getElement(fatVert.uniqueTangentIndex);
					subMeshBinormals[index] = uniqueVertexElements.uniqueBinormals.getElement(fatVert.uniqueBinormalIndex);
				}
#endif
				index++;
			}
			IonFbxSubMesh* pSubMesh = m_subMeshes + matId;

			printf("\t\t\tSubMesh[%2d] AABB:\t", matId);
			pSubMesh->GetAABB().Print();

			IonFbxMaterial* pMaterial = materials[matId];
			pSubMesh->SetMaterial(pMaterial);
			pSubMesh->SetNumVertices(vertexCount);
			VertexBuffer* vertexBuffer = pSubMesh->getVertexBuffer();
			vertexBuffer->setNumStreams(1);
			vertexBuffer->addElement(0, positionElement);
			vertexBuffer->addElement(0, normalElement);
			if (bBumpMapped)
			{
				vertexBuffer->addElement(0, tangentElement);
				vertexBuffer->addElement(0, binormalElement);
			}
			vertexBuffer->addElement(0, texCoordElement);
			if (bSkinned)
			{
				vertexBuffer->addElement(0, boneIndexElement);
				vertexBuffer->addElement(0, boneWeightElement);
			}
			vertexBuffer->setNumVertices(vertexCount);
			vertexBuffer->setData(0, positionElement, subMeshSrcVertices, sizeof(Vertex));
			vertexBuffer->setData(0, normalElement, subMeshNormals, sizeof(Vertex));
			if (bBumpMapped)
			{
				vertexBuffer->setData(0, tangentElement, subMeshTangents, sizeof(Vertex));
				vertexBuffer->setData(0, binormalElement, subMeshBinormals, sizeof(Vertex));
			}
			vertexBuffer->setData(0, texCoordElement, subMeshUVs, sizeof(TexCoord));
			if (bSkinned)
			{
				vertexBuffer->setData(0, boneIndexElement, subMeshBoneIndices, 16);
				vertexBuffer->setData(0, boneWeightElement, subMeshBoneWeights, 16);
			}
#if 1			
			// Re-index geometry
			uint32_t indexCount = triangleCount * 3;
			uint32_t* indices = new uint32_t[indexCount]; // triangleCount * 3];
			ReindexGeometry(pMesh, srcVertices, indices, matId, uniqueVertexElements, indexRemappingTable);  
			// Get rid of temporary maps.
			indexRemappingTable.clear();
			uniqueVerts.clear();	

			reng::Import::IndexBuffer* indexBuffer = new reng::Import::IndexBuffer(triangleCount*3, reng::Import::k32Bit);
			indexBuffer->writeIndices(indices);
			pSubMesh->SetIndexBuffer(indexBuffer);
#if 1
			{
//				ion::PerfTimer timer;
//				timer.Start();

				VCache vCache;
				vCache.Initialise(vertexBuffer, indexBuffer);	//((uint32_t*)pSubMesh->GetFaces());
				vCache.OptimiseIndices(indexBuffer);
				vCache.OptimiseVertices(vertexBuffer, indexBuffer);
				vCache.Shutdown();

//				timer.Stop();
//				float timeMilli = timer.GetElapsedMillis();
//				printf("Mesh Optimisation Time = %.3fms\n", timeMilli);
				
			}
#endif
			delete [] indices;
#endif
			if (bSkinned)
			{
				delete [] subMeshBoneIndices;
				delete [] subMeshBoneWeights;
			}
			delete [] subMeshUVs;
			delete [] subMeshNormals;
			delete [] subMeshSrcVertices;
			
		}
#endif
//		delete [] materials;
    }
    
	template<typename T>
	int32_t getFbxGeometryElement(T* element, uint32_t vertexIndex, uint32_t polyVertexIndex, float& x, float& y, float& z)
	{
		int32_t ret = -1;
		switch (element->GetMappingMode())
		{
		case FbxGeometryElement::eByControlPoint:
		{
			switch (element->GetReferenceMode())
			{
			case FbxGeometryElement::eDirect:
			{
				x = static_cast<float>(element->GetDirectArray().GetAt(vertexIndex).mData[0]);
				y = static_cast<float>(element->GetDirectArray().GetAt(vertexIndex).mData[1]);
				z = static_cast<float>(element->GetDirectArray().GetAt(vertexIndex).mData[2]);
				ret = vertexIndex;
				break;
			}
			case FbxGeometryElement::eIndexToDirect:
			{
				int index = element->GetIndexArray().GetAt(vertexIndex);
				x = static_cast<float>(element->GetDirectArray().GetAt(index).mData[0]);
				y = static_cast<float>(element->GetDirectArray().GetAt(index).mData[1]);
				z = static_cast<float>(element->GetDirectArray().GetAt(index).mData[2]);
				ret = index;
				break;
			}
			default:
				AssertMsg(1, "Invalid Reference Mode.\n");
			}
			break;
		}
		case FbxGeometryElement::eByPolygonVertex:
		{
			switch (element->GetReferenceMode())
			{
			case FbxGeometryElement::eDirect:
			{
				x = static_cast<float>(element->GetDirectArray().GetAt(polyVertexIndex).mData[0]);
				y = static_cast<float>(element->GetDirectArray().GetAt(polyVertexIndex).mData[1]);
				z = static_cast<float>(element->GetDirectArray().GetAt(polyVertexIndex).mData[2]);
				ret = polyVertexIndex;
				break;
			}
			case FbxGeometryElement::eIndexToDirect:
			{
				int index = element->GetIndexArray().GetAt(polyVertexIndex);
				x = static_cast<float>(element->GetDirectArray().GetAt(index).mData[0]);
				y = static_cast<float>(element->GetDirectArray().GetAt(index).mData[1]);
				z = static_cast<float>(element->GetDirectArray().GetAt(index).mData[2]);
				ret = index;
				break;
			}
			default:
				AssertMsg(1, "Invalid Reference Mode.\n");
			}
			break;
		}
		break;
		default:
			AssertMsg(1, "Invalid Mapping Mode.\n");
		}
		return ret;
	}

	uint32_t IonFbxMesh::GenerateFatVerts(FbxMesh *pMesh, Vertex4Bone *pSrcVertices, uint32_t matId, UniqueVertexElements& vertexElements, std::map<uint64_t, FbxFatVert>& uniqueVerts)
	{
		//CLR: Obtain texture UV scaling values.  This is a bit of a hack on a single channel.  It may be necessary to do this properly on a per texture basis.
		float textureScaleU = m_materials[matId]->m_uvScale[0];
		float textureScaleV = m_materials[matId]->m_uvScale[1];

		FbxSurfaceMaterial *lMaterial = pMesh->GetNode()->GetMaterial(matId);
		int lTextureIndex = -1;

		struct MappingScale
		{
			float x, y;
		};

		union MappingType
		{
			MappingScale scale;
			uint64_t packed;
		};

		std::set<uint64_t> mappingTypes;
		FBXSDK_FOR_EACH_TEXTURE(lTextureIndex)
		{	
			FbxProperty lProperty = lMaterial->FindProperty(FbxLayerElement::sTextureChannelNames[lTextureIndex]);
			if (lProperty.IsValid())
			{
				int lTextureCount = lProperty.GetSrcObjectCount<FbxTexture>();	// FbxTexture::ClassId);
				for (int j = 0; j < lTextureCount; ++j)
				{
					//Here we have to check if it's layeredtextures, or just textures:
					FbxLayeredTexture *lLayeredTexture = FbxCast <FbxLayeredTexture>(lProperty.GetSrcObject<FbxLayeredTexture>(j));
					if (lLayeredTexture)
					{
					}
					else
					{
						FbxTexture* lTexture = FbxCast <FbxTexture>(lProperty.GetSrcObject<FbxTexture>(0));
						if (lTexture)
						{
							FbxFileTexture * lFileTexture = FbxCast<FbxFileTexture>(lTexture);
							char* texName = NULL;	//= (char*)lCurrentTexture->GetFileName();
							if (lFileTexture && !lFileTexture->GetUserDataPtr())
							{
								textureScaleU = (float)lFileTexture->GetScaleU();
								textureScaleV = (float)lFileTexture->GetScaleV();
								MappingType mappingType;
								mappingType.scale.x = textureScaleU;
								mappingType.scale.y = textureScaleV;
								mappingTypes.insert(mappingType.packed);
							}
						}
					}
				}
			}
		}
		printf("Material needs %d texture streams\n", (uint32_t)mappingTypes.size());
/*
		FbxSurfaceMaterial *lMaterial = pMesh->GetNode()->GetMaterial(matId);
		FbxProperty lProperty = lMaterial->FindProperty(FbxLayerElement::sTextureChannelNames[0]);
		if (lProperty.IsValid())
		{
			FbxTexture* lTexture = FbxCast <FbxTexture>(lProperty.GetSrcObject<FbxTexture>(0));
			if (lTexture)
			{
				FbxFileTexture * lFileTexture = FbxCast<FbxFileTexture>(lTexture);
				char* texName = NULL;	//= (char*)lCurrentTexture->GetFileName();
				if (lFileTexture && !lFileTexture->GetUserDataPtr())
				{
					textureScaleU = lFileTexture->GetScaleU();
					textureScaleV = lFileTexture->GetScaleV();
				}
			}
		}
*/
        Vertex4Bone *vertexArray = pSrcVertices;
		FbxLayerElementArrayTemplate<FbxVector2>* lUVArray = NULL;    
		pMesh->GetTextureUV(&lUVArray, FbxLayerElement::eTextureDiffuse); 

		int uv2Id = 1;
		const FbxGeometryElementUV* pUv = 0;
		FbxLayerElementArrayTemplate<FbxVector2>* lUV2Array = NULL;
		if (uv2Id < pMesh->GetLayerCount())
		{
			const FbxLayer* lLayer = pMesh->GetLayer(uv2Id);
			pUv = (const FbxGeometryElementUV*)lLayer->GetUVs(FbxLayerElement::eTextureDiffuse);
			if (pUv)
			{
	//			printf("Found 2nd UV set\n");
				lUV2Array = &pUv->GetDirectArray();
			}
		}
		FbxArray<FbxVector4> pVertexNormals;
		pMesh->GetPolygonVertexNormals(pVertexNormals);
//		FbxArray<FbxVector4> pTangents;
		FbxLayerElementArrayTemplate<FbxVector4>* pTangents = nullptr;;
		pMesh->GetTangents(&pTangents);

		int numNormals = pMesh->GetElementNormalCount();
		int numTangents = pMesh->GetElementTangentCount();
		int numBinormals = pMesh->GetElementBinormalCount();
		FbxGeometryElementNormal* leNormal = pMesh->GetElementNormal(0);
		FbxGeometryElementTangent* leTangent = pMesh->GetElementTangent(0);
		FbxGeometryElementBinormal* leBinormal = pMesh->GetElementBinormal(0);
        FbxGeometryElementMaterial* leMat = pMesh->GetElementMaterial(0);
		
 		int polyCount = pMesh->GetPolygonCount();
		

		uint32_t polyVertexCounter = 0;
		uint32_t triangleCount = 0;

		for (int polygonIndex = 0; polygonIndex < polyCount; polygonIndex++)
        {
            int materialIndex = leMat->GetIndexArray().GetAt(polygonIndex);
			if (materialIndex != matId)
				continue;
			
			triangleCount++;
            int vertexCount = pMesh->GetPolygonSize( polygonIndex );
            for ( int polyVertexIndex = 0; polyVertexIndex < vertexCount; polyVertexIndex++ )
            {				
                int polyUVIndex = pMesh->GetTextureUVIndex( polygonIndex, polyVertexIndex);
				float u = (float)lUVArray->GetAt(polyUVIndex).mData[0];
				float v = 1.0f - (float)lUVArray->GetAt(polyUVIndex).mData[1];
				TexCoord uv(u*textureScaleU, v*textureScaleV);
				uint32_t uvIndex0 = vertexElements.uniqueUVs.addElement(uv);
				uint32_t uvIndex1 = -1;
				float u1 = 0.0f;
				float v1 = 0.0f;
				if (lUV2Array)
				{
					u1 = (float)lUV2Array->GetAt(polyUVIndex).mData[0];
					v1 = (float)lUV2Array->GetAt(polyUVIndex).mData[1];
					TexCoord uv1(u, v);
					uvIndex1 = vertexElements.uniqueUVs.addElement(uv1);
				}
				
                int vertexIndex = pMesh->GetPolygonVertex( polygonIndex, polyVertexIndex );
                float x = vertexArray[vertexIndex].srcVertex.x;
                float y = vertexArray[vertexIndex].srcVertex.y;
                float z = vertexArray[vertexIndex].srcVertex.z;

				float nx = 0.0f, ny = 0.0f, nz = 0.0f;
				int32_t normalIndex = getFbxGeometryElement<FbxGeometryElementNormal>(leNormal, vertexIndex, polyVertexCounter, nx, ny, nz);
				Vertex vertexNormal(nx, ny, nz);
				uint32_t uniqueNormalIndex = vertexElements.uniqueNormals.addElement(vertexNormal);
//				ion::print("Poly %d: Vertex (%d) normalIndex = %d -> (vert: %.3f, %.3f, %.3f), (norm: %.3f, %.3f, %.3f)\n", polygonIndex, vertexIndex, uniqueNormalIndex, x, y, z, nx, ny, nz);
				int32_t tangentIndex = -1;
				if (numTangents)
				{
					float tx = 0.0f, ty = 0.0f, tz = 0.0f;
					getFbxGeometryElement<FbxGeometryElementTangent>(leTangent, vertexIndex, polyVertexCounter, tx, ty, tz);
					Vertex vertexTangent(tx, ty, tz);
					tangentIndex = vertexElements.uniqueTangents.addElement(vertexTangent);
				}
				int32_t binormalIndex = -1;
				if (numBinormals)
				{
					float bx = 0.0f, by = 0.0f, bz = 0.0f;
					getFbxGeometryElement<FbxGeometryElementBinormal>(leBinormal, vertexIndex, polyVertexCounter, bx, by, bz);
					Vertex vertexBinormal(bx, by, bz);
					binormalIndex = vertexElements.uniqueBinormals.addElement(vertexBinormal);
				}
				FbxFatVert newFatVert;
				newFatVert.uniqueVertexIndex = vertexIndex;
				newFatVert.uniqueNormalIndex = uniqueNormalIndex;
				newFatVert.uniqueUvIndices[0] = uvIndex0;
				newFatVert.uniqueUvIndices[1] = uvIndex1;
				newFatVert.uniqueTangentIndex = tangentIndex;
				newFatVert.uniqueBinormalIndex = binormalIndex;
//				newFatVert.uniqueBoneIndices = Vector4i(vertexArray[vertexIndex].boneIndices[0], vertexArray[vertexIndex].boneIndices[1], vertexArray[vertexIndex].boneIndices[2], vertexArray[vertexIndex].boneIndices[3]);
//				newFatVert.uniqueBoneWeights = Vector4f(vertexArray[vertexIndex].boneWeights[0], vertexArray[vertexIndex].boneWeights[1], vertexArray[vertexIndex].boneWeights[2], vertexArray[vertexIndex].boneWeights[3]);
#if 0
                FbxFatVert newFatVert;
				newFatVert.vertex.x = x;
				newFatVert.vertex.y = y;
				newFatVert.vertex.z = z;
				newFatVert.normal.x = nx;
				newFatVert.normal.y = ny;
				newFatVert.normal.z = nz;
/*
				newFatVert.tangent.x = tx;
				newFatVert.tangent.y = ty;
				newFatVert.tangent.z = tz;
				newFatVert.binormal.x = bx;
				newFatVert.binormal.y = by;
				newFatVert.binormal.z = bz;
*/
				//				printf("tri[%d] vertex[%d] = v(%.3f, %.3f, %.3f), n(%.3f, %.3f, %.3f)\n", polygonIndex, polyVertexIndex, x, y, z, nx, ny, nz);
				newFatVert.texCoord0.u = u;
				newFatVert.texCoord0.v = 1.0f - v;
				newFatVert.texCoord1.u = u1;
				newFatVert.texCoord1.v = 1.0f - v1;
                newFatVert.weights[0] = vertexArray[vertexIndex].boneWeights[0];
                newFatVert.weights[1] = vertexArray[vertexIndex].boneWeights[1];
                newFatVert.weights[2] = vertexArray[vertexIndex].boneWeights[2];
                newFatVert.weights[3] = vertexArray[vertexIndex].boneWeights[3];
                newFatVert.bones[0] = vertexArray[vertexIndex].boneIndices[0];
                newFatVert.bones[1] = vertexArray[vertexIndex].boneIndices[1];
                newFatVert.bones[2] = vertexArray[vertexIndex].boneIndices[2];
                newFatVert.bones[3] = vertexArray[vertexIndex].boneIndices[3];
#endif
                // Get Vertex Hash
				uint64_t vertexHash = Hash64(&newFatVert, sizeof(FbxFatVert));
                //Check to see if Vertex has already been added to the Map
				std::map<uint64_t, FbxFatVert>::iterator find = uniqueVerts.find(vertexHash);
				if(find != uniqueVerts.end())
				{
					//                    printf("Vertex already in uniqueVerts no need to add it again\n");
				}
				else
				{
					uniqueVerts[vertexHash] = newFatVert;
				}
				polyVertexCounter++;
            }
        }
		return triangleCount;
	}    
	
    void IonFbxMesh::ReindexGeometry(FbxMesh *pMesh, Vertex4Bone *srcVertices, uint32_t *indices, uint32_t matId, UniqueVertexElements& vertexElements, std::map<uint64_t, uint32_t>&indexRemappingTable )
    {
		float textureScaleU = m_materials[matId]->m_uvScale[0];
		float textureScaleV = m_materials[matId]->m_uvScale[0];

		FbxSurfaceMaterial *lMaterial = pMesh->GetNode()->GetMaterial(matId);
		FbxProperty lProperty = lMaterial->FindProperty(FbxLayerElement::sTextureChannelNames[0]);
		if (lProperty.IsValid())
		{
			FbxTexture* lTexture = FbxCast <FbxTexture>(lProperty.GetSrcObject<FbxTexture>(0));
			if (lTexture)
			{
				FbxFileTexture * lFileTexture = FbxCast<FbxFileTexture>(lTexture);
				char* texName = NULL;	//= (char*)lCurrentTexture->GetFileName();
				if (lFileTexture && !lFileTexture->GetUserDataPtr())
				{
					textureScaleU = (float)lFileTexture->GetScaleU();
					textureScaleV = (float)lFileTexture->GetScaleV();
				}
			}
		}

		Vertex4Bone *vertexArray = srcVertices;
		FbxLayerElementArrayTemplate<FbxVector2>* lUVArray = NULL;    
		pMesh->GetTextureUV(&lUVArray, FbxLayerElement::eTextureDiffuse);
		
		int uv2Id = 1;
		const FbxGeometryElementUV* pUv = 0;
		FbxLayerElementArrayTemplate<FbxVector2>* lUV2Array = NULL;
		if (uv2Id < pMesh->GetLayerCount())
		{
			const FbxLayer* lLayer = pMesh->GetLayer(uv2Id);
			pUv = (const FbxGeometryElementUV*)lLayer->GetUVs(FbxLayerElement::eTextureDiffuse);
			if (pUv)
			{
//				printf("Found 2nd UV set\n");
				lUV2Array = &pUv->GetDirectArray();
			}
		}
		
        //Setup temporary array for index buffer data
        uint32_t *pIndex = indices;
  		
		int numNormals = pMesh->GetElementNormalCount();
		int numTangents = pMesh->GetElementTangentCount();
		int numBinormals = pMesh->GetElementBinormalCount();
		FbxGeometryElementNormal* leNormal = pMesh->GetElementNormal(0);
		FbxGeometryElementTangent* leTangent = pMesh->GetElementTangent(0);
		FbxGeometryElementBinormal* leBinormal = pMesh->GetElementBinormal(0);
		FbxGeometryElementMaterial* leMat = pMesh->GetElementMaterial(0);

		int polyCount = pMesh->GetPolygonCount();
		uint32_t polyVertexCounter = 0;

        //Iterate over mesh, obtain unique vertex index for each existing mesh index.
        for ( int polygonIndex = 0; polygonIndex < polyCount; polygonIndex++ )
        {
            int materialIndex = leMat->GetIndexArray().GetAt(polygonIndex);
			if (materialIndex != matId)
				continue;
			
            int vertexCount = pMesh->GetPolygonSize( polygonIndex );
            
            for ( int polyVertexIndex = 0; polyVertexIndex < vertexCount; polyVertexIndex++ )
            {
/*
                int polyUVIndex = pMesh->GetTextureUVIndex( polygonIndex, polyVertexIndex);
				float u = lUVArray->GetAt(polyUVIndex).mData[0];
				float v = lUVArray->GetAt(polyUVIndex).mData[1];
				
				float u1 = 0.0f;
				float v1 = 0.0f;
				if (lUV2Array)
				{
					u1 = lUV2Array->GetAt(polyUVIndex).mData[0];
					v1 = lUV2Array->GetAt(polyUVIndex).mData[1];
				}

                int vertexIndex = pMesh->GetPolygonVertex( polygonIndex, polyVertexIndex );
                float x = srcVertices[vertexIndex].srcVertex.X();
                float y = srcVertices[vertexIndex].srcVertex.Y();
                float z = srcVertices[vertexIndex].srcVertex.Z();
				
				FbxVector4 normal;
				pMesh->GetPolygonVertexNormal(polygonIndex, polyVertexIndex, normal);	
				float nx = normal.mData[0];
				float ny = normal.mData[1];
				float nz = normal.mData[2];
*/				
                int polyUVIndex = pMesh->GetTextureUVIndex( polygonIndex, polyVertexIndex);
				float u = (float)lUVArray->GetAt(polyUVIndex).mData[0];
				float v = 1.0f - (float)lUVArray->GetAt(polyUVIndex).mData[1];
				TexCoord uv(u*textureScaleU, v*textureScaleV);
				uint32_t uvIndex0 = vertexElements.uniqueUVs.addElement(uv);
				uint32_t uvIndex1 = -1;
				float u1 = 0.0f;
				float v1 = 0.0f;
				if (lUV2Array)
				{
					u1 = (float)lUV2Array->GetAt(polyUVIndex).mData[0];
					v1 = (float)lUV2Array->GetAt(polyUVIndex).mData[1];
					TexCoord uv1(u, v);
					uvIndex1 = vertexElements.uniqueUVs.addElement(uv1);
				}
				
                int vertexIndex = pMesh->GetPolygonVertex( polygonIndex, polyVertexIndex );
                float x = vertexArray[vertexIndex].srcVertex.x;
                float y = vertexArray[vertexIndex].srcVertex.y;
                float z = vertexArray[vertexIndex].srcVertex.z;

				float nx = 0.0f, ny = 0.0f, nz = 0.0f;
				int32_t normalIndex = getFbxGeometryElement<FbxGeometryElementNormal>(leNormal, vertexIndex, polyVertexCounter, nx, ny, nz);
				Vertex vertexNormal(nx, ny, nz);
				uint32_t uniqueNormalIndex = vertexElements.uniqueNormals.addElement(vertexNormal);
//				ion::print("Poly %d: Vertex (%d) normalIndex = %d -> (vert: %.3f, %.3f, %.3f), (norm: %.3f, %.3f, %.3f)\n", polygonIndex, vertexIndex, uniqueNormalIndex, x, y, z, nx, ny, nz);
				int32_t tangentIndex = -1;
				if (numTangents)
				{
					float tx = 0.0f, ty = 0.0f, tz = 0.0f;
					getFbxGeometryElement<FbxGeometryElementTangent>(leTangent, vertexIndex, polyVertexCounter, tx, ty, tz);
					Vertex vertexTangent(tx, ty, tz);
					tangentIndex = vertexElements.uniqueTangents.addElement(vertexTangent);
				}
				int32_t binormalIndex = -1;
				if (numBinormals)
				{
					float bx = 0.0f, by = 0.0f, bz = 0.0f;
					getFbxGeometryElement<FbxGeometryElementBinormal>(leBinormal, vertexIndex, polyVertexCounter, bx, by, bz);
					Vertex vertexBinormal(bx, by, bz);
					binormalIndex = vertexElements.uniqueBinormals.addElement(vertexBinormal);
				}
				FbxFatVert newFatVert;
				newFatVert.uniqueVertexIndex = vertexIndex;
				newFatVert.uniqueNormalIndex = uniqueNormalIndex;
				newFatVert.uniqueUvIndices[0] = uvIndex0;
				newFatVert.uniqueUvIndices[1] = uvIndex1;
				newFatVert.uniqueTangentIndex = tangentIndex;
				newFatVert.uniqueBinormalIndex = binormalIndex;
//				newFatVert.uniqueBoneIndices = Vector4i(vertexArray[vertexIndex].boneIndices[0], vertexArray[vertexIndex].boneIndices[1], vertexArray[vertexIndex].boneIndices[2], vertexArray[vertexIndex].boneIndices[3]);
//				newFatVert.uniqueBoneWeights = Vector4f(vertexArray[vertexIndex].boneWeights[0], vertexArray[vertexIndex].boneWeights[1], vertexArray[vertexIndex].boneWeights[2], vertexArray[vertexIndex].boneWeights[3]);
#if 0
				FbxFatVert newFatVert;
				newFatVert.vertex.x = x;
				newFatVert.vertex.y = y;
				newFatVert.vertex.z = z;
				newFatVert.normal.x = nx;
				newFatVert.normal.y = ny;
				newFatVert.normal.z = nz;
				newFatVert.texCoord0.u = u;
				newFatVert.texCoord0.v = 1.0f - v;
				newFatVert.texCoord1.u = u1;
				newFatVert.texCoord1.v = 1.0f - v1;
                newFatVert.weights[0] = srcVertices[vertexIndex].boneWeights[0];
                newFatVert.weights[1] = srcVertices[vertexIndex].boneWeights[1];
                newFatVert.weights[2] = srcVertices[vertexIndex].boneWeights[2];
                newFatVert.weights[3] = srcVertices[vertexIndex].boneWeights[3];
                newFatVert.bones[0] = srcVertices[vertexIndex].boneIndices[0];
                newFatVert.bones[1] = srcVertices[vertexIndex].boneIndices[1];
                newFatVert.bones[2] = srcVertices[vertexIndex].boneIndices[2];
                newFatVert.bones[3] = srcVertices[vertexIndex].boneIndices[3];
#endif

                // Get Vertex Hash
				uint64_t vertexHash = Hash64(&newFatVert, sizeof(FbxFatVert));
                //Search indexRemappingTable for vertexHash to obtain new index;
				std::map<uint64_t, uint32_t>::iterator find = indexRemappingTable.find(vertexHash);
				if(find != indexRemappingTable.end())
				{
					uint32_t index = (*find).second;
                    *pIndex++ = index;
				}    
				polyVertexCounter++;
            }
        }        
    }
	
    void IonFbxMesh::Skin( )
    {
/*		
        glBindBufferARB( GL_ARRAY_BUFFER_ARB, m_glDynamicVertexBuffer );
        Vector4 *dstVertices = static_cast<Vector4*>(glMapBuffer(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY));
        
        int vertexCount = m_vertexCount;
        for (int i = 0; i < vertexCount; i++) 
        {
//            Vertex4Bone vertex = m_srcVertices[i];
            Vector4& srcVertex = m_srcVertices[i];	//vertex.srcVertex;
            Vector4& dstVertex = dstVertices[i];
            
            Mat44    result = m_bonePalette[m_boneIndices[i].x] * m_boneWeights[i].X();
            result = result + m_bonePalette[m_boneIndices[i].y] * m_boneWeights[i].Y();
            result = result + m_bonePalette[m_boneIndices[i].z] * m_boneWeights[i].Z();
            result = result + m_bonePalette[m_boneIndices[i].w] * m_boneWeights[i].W();
            dstVertex = result * srcVertex;
        }
        glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
 */
    }

	static const uint32_t kTEX_color_map = hashString("TEX_color_map");
	static const uint32_t kTEX_normal_map = hashString("TEX_normal_map");
	static const uint32_t kTEX_metallic_map = hashString("TEX_metallic_map");
	static const uint32_t kTEX_roughness_map = hashString("TEX_roughness_map");
	static const uint32_t kTEX_emissive_map = hashString("TEX_emissive_map");
	static const uint32_t kTEX_ao_map = hashString("TEX_ao_map");

	void IonFbxMesh::ConvertMeshMaterials(FbxMesh* pMesh, uint32_t numMaterials, IonFbxMaterial** materials, bool bSkinned)
	{
		FbxPropertyT<FbxDouble3> lKFbxDouble3;
		FbxPropertyT<FbxDouble> lKFbxDouble1;
		FbxColor theColor;

		int materialElementCount = pMesh->GetElementMaterialCount();
		for (uint32_t materialIndex = 0; materialIndex < numMaterials; ++materialIndex)
		{
			FbxSurfaceMaterial *lMaterial = pMesh->GetNode()->GetMaterial(materialIndex);
			std::string materialName = lMaterial->GetName();
			IonFbxMaterial* pMaterial = 0;	// new IonFbxMaterial();
			if (!pMaterial)
			{
				IonFbxMaterial* pIonFbxMaterial = 0;
				pIonFbxMaterial = new IonFbxMaterial();
				pIonFbxMaterial->SetName(static_cast<std::string>(lMaterial->GetName()));
				printf("\t\tMaterial: %s (0x%08x) [ADD]\n", (char *)pIonFbxMaterial->GetName().c_str(), pIonFbxMaterial->GetHash());

				const FbxImplementation* lImplementation = GetImplementation(lMaterial, FBXSDK_IMPLEMENTATION_HLSL);
				FbxString lImplemenationType = "HLSL";
				if (!lImplementation)
				{
					lImplementation = GetImplementation(lMaterial, FBXSDK_IMPLEMENTATION_SFX);
					lImplemenationType = "SFX";
				}
				FbxClassId mtlClassId = lMaterial->GetClassId();
				FbxClassId phoneClassId = FbxSurfacePhong::ClassId;
				if (lImplementation)
				{
					//Now we have a hardware shader, let's read it
					pIonFbxMaterial->SetPhysicallyBased(true);
					FBXSDK_printf("\t\t\tHardware Shader Type: %s\n", lImplemenationType.Buffer());
					const FbxBindingTable* lRootTable = lImplementation->GetRootTable();
					FbxString lFileName = lRootTable->DescAbsoluteURL.Get();
					FbxString lFileName2 = lRootTable->DescRelativeURL.Get();
					FBXSDK_printf("\t\t\tShader: %s\n", lFileName2.Buffer());
					FbxString lTechniqueName = lRootTable->DescTAG.Get();
					FBXSDK_printf("\t\t\tTechnique: %s\n", lTechniqueName.Buffer());

					const FbxBindingTable* lTable = lImplementation->GetRootTable();
					size_t lEntryNum = lTable->GetEntryCount();

					for (int i = 0; i < (int)lEntryNum; ++i)
					{
						const FbxBindingTableEntry& lEntry = lTable->GetEntry(i);
						const char* lEntrySrcType = lEntry.GetEntryType(true);
						FbxProperty lFbxProp;


						//						FbxString lTest = lEntry.GetSource();
						//						FBXSDK_printf("\t\t\tEntry: %s\n", lTest.Buffer());
						FbxString lTest2 = lEntry.GetDestination();
						FBXSDK_printf("\t\t\tEntry: %s -> ", lTest2.Buffer());

 						if (strcmp(FbxPropertyEntryView::sEntryType, lEntrySrcType) == 0)
						{
							lFbxProp = lMaterial->FindPropertyHierarchical(lEntry.GetSource());
							if (!lFbxProp.IsValid())
							{
								lFbxProp = lMaterial->RootProperty.FindHierarchical(lEntry.GetSource());
							}
						}
						else if (strcmp(FbxConstantEntryView::sEntryType, lEntrySrcType) == 0)
						{
							lFbxProp = lImplementation->GetConstants().FindHierarchical(lEntry.GetSource());
						}
						if (lFbxProp.IsValid())
						{
							//							ion::print("Type: %s\n", type.);
							if (lFbxProp.GetSrcObjectCount<FbxTexture>() > 0)
							{
								//do what you want with the textures
								for (int j = 0; j < lFbxProp.GetSrcObjectCount<FbxFileTexture>(); ++j)
								{
									FbxFileTexture* lTex = lFbxProp.GetSrcObject<FbxFileTexture>(j);
									//									FBXSDK_printf("\t\t\tFile Texture: %s\n", lTex->GetFileName());
									FBXSDK_printf("%s (File)\n", lTex->GetRelativeFileName());
									const char* texName = (const char*)lTex->GetRelativeFileName();	// GetFileName();
									char* filenameBuffer = (char*)alloca(strlen(texName));
									const char* lastBackslash = strrchr(texName, (int32_t)'\\');
									if (lastBackslash != NULL)
									{
										strcpy(filenameBuffer, lastBackslash + 1);
									}
									IonFbxTexture* pTexture = new IonFbxTexture(filenameBuffer);
									IonFbxTexture* pAdditionalTexture = nullptr;				//CLR: Used for Heightmap...
									printf("%s (0x%08x)\n", filenameBuffer, pTexture->GetHash());
									uint32_t texTypeHash = hashString(lTest2.Buffer());
									bool validTexture = false;
									if (texTypeHash == kTEX_color_map) {
										pIonFbxMaterial->SetDiffuseMap(pTexture);
										validTexture = true;
									}
									else if (texTypeHash == kTEX_normal_map) {
										pIonFbxMaterial->SetNormalMap(pTexture);
										validTexture = true;
									}
									else if (texTypeHash == kTEX_metallic_map) {
										pIonFbxMaterial->SetMetallicMap(pTexture);
										validTexture = true;
									}
									else if (texTypeHash == kTEX_roughness_map) {
										pIonFbxMaterial->SetRoughnessMap(pTexture);
										validTexture = true;
									}
									else if (texTypeHash == kTEX_emissive_map) {
										pIonFbxMaterial->SetEmissiveMap(pTexture);
										validTexture = true;
									}
									else if (texTypeHash == kTEX_ao_map) {
										pIonFbxMaterial->SetAmbientMap(pTexture);
										validTexture = true;
									}
									else {

									}
									if (validTexture)
									{
										_textures.insert(pTexture);
									}
								}
								for (int j = 0; j < lFbxProp.GetSrcObjectCount<FbxLayeredTexture>(); ++j)
								{
									FbxLayeredTexture *lTex = lFbxProp.GetSrcObject<FbxLayeredTexture>(j);
									FBXSDK_printf("%s (Layered)\n", lTex->GetName());
								}
								for (int j = 0; j < lFbxProp.GetSrcObjectCount<FbxProceduralTexture>(); ++j)
								{
									FbxProceduralTexture *lTex = lFbxProp.GetSrcObject<FbxProceduralTexture>(j);
									FBXSDK_printf("%s (Procedural)\n", lTex->GetName());
								}
							}
							else
							{
								FbxDataType lFbxType = lFbxProp.GetPropertyDataType();
								const char* propName = lFbxProp.GetNameAsCStr();

								if (FbxBoolDT == lFbxType)
								{
									DisplayBool("Bool: ", lFbxProp.Get<FbxBool>());
								}
								else if (FbxIntDT == lFbxType || FbxEnumDT == lFbxType)
								{
									DisplayInt("Int: ", lFbxProp.Get<FbxInt>());
								}
								else if (FbxFloatDT == lFbxType)
								{
									DisplayDouble("Float: ", lFbxProp.Get<FbxFloat>());
								}
								else if (FbxDoubleDT == lFbxType)
								{
									DisplayDouble("Double: ", lFbxProp.Get<FbxDouble>());
								}
								else if (FbxStringDT == lFbxType
									|| FbxUrlDT == lFbxType
									|| FbxXRefUrlDT == lFbxType)
								{
									DisplayString("String: ", lFbxProp.Get<FbxString>().Buffer());
								}
								else if (FbxDouble2DT == lFbxType)
								{
									FbxDouble2 lDouble2 = lFbxProp.Get<FbxDouble2>();
									FbxVector2 lVect;
									lVect[0] = lDouble2[0];
									lVect[1] = lDouble2[1];

									Display2DVector("2D vector: ", lVect);
									if (!strcmp(propName, "uv_scale"))
									{
										pIonFbxMaterial->SetUVScale((float)lDouble2[0], (float)lDouble2[1]);
									}
								}
								else if (FbxDouble3DT == lFbxType || FbxColor3DT == lFbxType)
								{
									FbxDouble3 lDouble3 = lFbxProp.Get<FbxDouble3>();

									FbxVector4 lVect;
									lVect[0] = lDouble3[0];
									lVect[1] = lDouble3[1];
									lVect[2] = lDouble3[2];
									Display3DVector("3D vector: ", lVect);
								}
								else if (FbxDouble4DT == lFbxType || FbxColor4DT == lFbxType)
								{
									FbxDouble4 lDouble4 = lFbxProp.Get<FbxDouble4>();
									FbxVector4 lVect;
									lVect[0] = lDouble4[0];
									lVect[1] = lDouble4[1];
									lVect[2] = lDouble4[2];
									lVect[3] = lDouble4[3];
									Display4DVector("4D vector: ", lVect);
								}
								else if (FbxDouble4x4DT == lFbxType)
								{
									FbxDouble4x4 lDouble44 = lFbxProp.Get<FbxDouble4x4>();
									for (int j = 0; j < 4; ++j)
									{
										FbxVector4 lVect;
										lVect[0] = lDouble44[j][0];
										lVect[1] = lDouble44[j][1];
										lVect[2] = lDouble44[j][2];
										lVect[3] = lDouble44[j][3];
										Display4DVector("4x4D vector: ", lVect);
									}
								}
								else if (FbxTransformMatrixDT == lFbxType)
								{
									FbxTransform lMatrix = lFbxProp.Get<FbxTransform>();
									FbxDouble4x4 lDouble44 = lFbxProp.Get<FbxDouble4x4>();
									for (int j = 0; j < 4; ++j)
									{
										FbxVector4 lVect;
										lVect[0] = lDouble44[j][0];
										lVect[1] = lDouble44[j][1];
										lVect[2] = lDouble44[j][2];
										lVect[3] = lDouble44[j][3];
										Display4DVector("4x4D vector: ", lVect);
									}
								}
							}
						}
					}
				}
				else if (lMaterial->GetClassId().Is(FbxSurfacePhong::ClassId))
				{
					// We found a Phong material.  Display its properties.

					// Display the Ambient Color
					lKFbxDouble3 = ((FbxSurfacePhong *)lMaterial)->Ambient;
					//					pIonFbxMaterial->SetAmbientColour(Colour(lKFbxDouble3.Get()[0], lKFbxDouble3.Get()[1], lKFbxDouble3.Get()[2]));
					pIonFbxMaterial->SetAmbientColor(0.2f, 0.2f, 0.2f);
					lKFbxDouble1 = ((FbxSurfacePhong *)lMaterial)->DiffuseFactor;

					// Display the Diffuse Color
					lKFbxDouble3 = ((FbxSurfacePhong *)lMaterial)->Diffuse;
					float diffuseFactor = (float)lKFbxDouble1.Get();
					float diffuseR = (float)lKFbxDouble3.Get()[0];
					float diffuseG = (float)lKFbxDouble3.Get()[1];
					float diffuseB = (float)lKFbxDouble3.Get()[2];
					pIonFbxMaterial->SetDiffuseColor((float)lKFbxDouble1.Get() * (float)lKFbxDouble3.Get()[0], (float)lKFbxDouble1.Get() * (float)lKFbxDouble3.Get()[1], (float)lKFbxDouble1.Get() * (float)lKFbxDouble3.Get()[2]);
//					pIonFbxMaterial->SetDiffuseColor(lKFbxDouble1.Get() * powf(lKFbxDouble3.Get()[0], 2.2f), lKFbxDouble1.Get() * powf(lKFbxDouble3.Get()[1], 2.2f), lKFbxDouble1.Get() * powf(lKFbxDouble3.Get()[2], 2.2f));

					// Display the Specular Color (unique to Phong materials)
					lKFbxDouble3 = ((FbxSurfacePhong *)lMaterial)->Specular;
					pIonFbxMaterial->SetSpecularColor((float)lKFbxDouble3.Get()[0], (float)lKFbxDouble3.Get()[1], (float)lKFbxDouble3.Get()[2]);
					/*
					// Display the Emissive Color
					lKFbxDouble3 =((FbxSurfacePhong *) lMaterial)->Emissive;
					pIonFbxMaterial->m_emissiveColor = Vector(lKFbxDouble3.Get()[0], lKFbxDouble3.Get()[1], lKFbxDouble3.Get()[2]);
					*/
					// Display the Shininess
					lKFbxDouble1 = ((FbxSurfacePhong *)lMaterial)->Shininess;
					pIonFbxMaterial->SetSpecularExponent((float)lKFbxDouble1.Get());

					//Opacity is Transparency factor now
					lKFbxDouble1 = ((FbxSurfacePhong*)lMaterial)->TransparencyFactor;
					float transparency = (float)lKFbxDouble1.Get();

					pIonFbxMaterial->SetSkinned(bSkinned);
					/*
					//Opacity is Transparency factor now
					lKFbxDouble1 =((FbxSurfacePhong *) lMaterial)->TransparencyFactor;
					pIonFbxMaterial->m_transparency = lKFbxDouble1.Get();

					// Display the Shininess
					lKFbxDouble1 =((FbxSurfacePhong *) lMaterial)->Shininess;
					pIonFbxMaterial->m_shininess = lKFbxDouble1.Get();

					// Display the Reflectivity
					lKFbxDouble1 =((FbxSurfacePhong *) lMaterial)->ReflectionFactor;
					pIonFbxMaterial->m_reflectivity = lKFbxDouble1.Get();
					*/
				}
				else if (lMaterial->GetClassId().Is(FbxSurfaceLambert::ClassId))
				{
				/*
				// We found a Lambert material. Display its properties.
				// Display the Ambient Color
				lKFbxDouble3=((FbxSurfaceLambert *)lMaterial)->Ambient;
				pIonFbxMaterial->m_ambientColor = Vector(lKFbxDouble3.Get()[0], lKFbxDouble3.Get()[1], lKFbxDouble3.Get()[2]);

				// Display the Diffuse Color
				lKFbxDouble3 =((FbxSurfaceLambert *)lMaterial)->Diffuse;
				pIonFbxMaterial->m_diffuseColor = Vector(lKFbxDouble3.Get()[0], lKFbxDouble3.Get()[1], lKFbxDouble3.Get()[2]);

				// Display the Emissive
				lKFbxDouble3 =((FbxSurfaceLambert *)lMaterial)->Emissive;
				pIonFbxMaterial->m_specularColor = Vector(lKFbxDouble3.Get()[0], lKFbxDouble3.Get()[1], lKFbxDouble3.Get()[2]);

				// Display the Opacity
				lKFbxDouble1 =((FbxSurfaceLambert *)lMaterial)->TransparencyFactor;
				pIonFbxMaterial->m_transparency = lKFbxDouble1.Get();
				*/
				}
				else
				{
				printf("Unknown Material type\n");
				}
				//go through all the possible textures
				if (lMaterial)
				{
					int lTextureIndex;
					FBXSDK_FOR_EACH_TEXTURE(lTextureIndex)
					{
						FbxProperty lProperty = lMaterial->FindProperty(FbxLayerElement::sTextureChannelNames[lTextureIndex]);
						if (lProperty.IsValid())
						{
							int lTextureCount = lProperty.GetSrcObjectCount<FbxTexture>();	// FbxTexture::ClassId);

							for (int j = 0; j < lTextureCount; ++j)
							{
								//Here we have to check if it's layeredtextures, or just textures:
								FbxLayeredTexture *lLayeredTexture = FbxCast <FbxLayeredTexture>(lProperty.GetSrcObject<FbxLayeredTexture>(j));
								if (lLayeredTexture)
								{
									printf("    Layered Texture: %d\n", j);
									FbxLayeredTexture *lLayeredTexture = FbxCast <FbxLayeredTexture>(lProperty.GetSrcObject<FbxLayeredTexture>(j));
									int lNbTextures = lLayeredTexture->GetSrcObjectCount<FbxTexture>();
									for (int k = 0; k < lNbTextures; ++k)
									{
										FbxTexture* lTexture = FbxCast <FbxTexture>(lLayeredTexture->GetSrcObject<FbxTexture>(k));
										if (lTexture)
										{
											//NOTE the blend mode is ALWAYS on the LayeredTexture and NOT the one on the texture.
											//Why is that?  because one texture can be shared on different layered textures and might
											//have different blend modes.

											FbxLayeredTexture::EBlendMode lBlendMode;
											lLayeredTexture->GetTextureBlendMode(k, lBlendMode);
											printf("    Textures for %s\n", lProperty.GetName().Buffer());
											printf("        Texture %d\n", k);
											//										DisplayTextureInfo(lTexture, (int) lBlendMode);   
										}

									}
								}
								else
								{
									//no layered texture simply get on the property
									FbxTexture* lTexture = FbxCast <FbxTexture>(lProperty.GetSrcObject<FbxTexture>(j));
									if (lTexture)
									{
										FbxFileTexture * lFileTexture = FbxCast<FbxFileTexture>(lTexture);
										char* texName = NULL;	//= (char*)lCurrentTexture->GetFileName();
										if (lFileTexture && !lFileTexture->GetUserDataPtr())
										{
											float textureScaleU = (float)lFileTexture->GetScaleU();
											float textureScaleV = (float)lFileTexture->GetScaleV();
											pIonFbxMaterial->SetUVScale(textureScaleU, textureScaleV);
											// Try to load the texture from absolute path
											texName = (char*)lFileTexture->GetRelativeFileName();	// GetFileName();
											size_t len = strlen(texName);
											char* filenameBuffer = (char*)alloca(len);
											char* lastBackslash = strrchr(texName, (int32_t)'\\');
											if (lastBackslash != NULL)
											{
												strcpy(filenameBuffer, lastBackslash + 1);
											}
											//											printf("ConvertMesh: DiffuseMap: %s\n", texName);
											//										pIonFbxMaterial->SetDiffuseTexture(ion::Texture::Create(texName));
											printf("\t\t\tTexture: ");
											//											ion::TexturePtr pTexture = ion::TextureManager::Get()->Find(texName);
											IonFbxTexture* pTexture = new IonFbxTexture(filenameBuffer);
											IonFbxTexture* pAdditionalTexture = nullptr;
											printf("%s (0x%08x), uScale = %.3f, vScale = %.3f\n", texName, pTexture->GetHash(), textureScaleU, textureScaleV);
											switch (lTextureIndex + FbxLayerElement::eTextureDiffuse)
											{
											case FbxLayerElement::eTextureDiffuse:
												pIonFbxMaterial->SetDiffuseMap(pTexture);
												break;
											case FbxLayerElement::eTextureNormalMap:
											{
												pIonFbxMaterial->SetNormalMap(pTexture);
//												char* tempBuffer = (char*)alloca(strlen(texName) + 1);
												char* tempBuffer = (char*)malloc(strlen(filenameBuffer) + 1);
												strcpy(tempBuffer, filenameBuffer);
												char *foundLower = strstr(tempBuffer, "normal");
												char *foundUpper = strstr(tempBuffer, "Normal");
												if (foundLower != nullptr || foundUpper != nullptr)
												{
													if (foundLower)
														memcpy(foundLower, "height", 6);
													else
														memcpy(foundUpper, "Height", 6);
													if (FILE *file = fopen(tempBuffer, "r"))
													{
														fclose(file);
														pAdditionalTexture = new IonFbxTexture(tempBuffer);
														pIonFbxMaterial->SetHeightMap(pAdditionalTexture);
													}
												}
												free(tempBuffer);
												tempBuffer = NULL;
												break;
											}
											case FbxLayerElement::eTextureAmbient:
												pIonFbxMaterial->SetAmbientMap(pTexture);
												break;
											case FbxLayerElement::eTextureEmissive:
												pIonFbxMaterial->SetEmissiveMap(pTexture);
												break;
											case FbxLayerElement::eTextureTransparency:
												pIonFbxMaterial->SetTransparency(true);
												break;
											}
											//											_textureHashes.insert(pTexture->GetHash().Get());
											_textures.insert(pTexture);
											if (pAdditionalTexture)
												_textures.insert(pAdditionalTexture);
										}
									}
								}
							}
						}//end if pProperty
					}

				}// end for lMaterialIndex   
				pMaterial = pIonFbxMaterial;
				//				pMaterial->BindToGL();
				//				ion::MaterialManager::Get()->Add(pMaterial);
			}
			else {
				printf("\t\tMaterial: %s (0x%08x) [REF]\n", (char *)pMaterial->GetName().c_str(), pMaterial->GetHash());
			}
			materials[materialIndex] = pMaterial;
			//			_materialHashes.insert(pMaterial->GetHash());
			_materials.insert(pMaterial);	//->GetHash());
											// Obtain Texture
											//        KFbxLayerElementTexture* lTextureLayer = pMesh->GetLayer(0)->GetDiffuseTextures();
			FbxTexture* lCurrentTexture = NULL;
			/*
			if(lMaterial)
			{
			FbxProperty lProperty;
			lProperty = lMaterial->FindProperty(FbxSurfaceMaterial::sDiffuse);
			if(lProperty.IsValid())
			{
			lCurrentTexture = FbxCast <FbxTexture>(lProperty.GetSrcObject(FbxTexture::ClassId, 0));
			if (lCurrentTexture)
			{
			FbxFileTexture * lFileTexture = FbxCast<FbxFileTexture>(lCurrentTexture);
			char* texName = NULL;	//= (char*)lCurrentTexture->GetFileName();
			if (lFileTexture && !lFileTexture->GetUserDataPtr())
			{
			// Try to load the texture from absolute path
			texName = (char*)lFileTexture->GetFileName();
			}
			//				char* texName = (char*)lCurrentTexture->GetFileName();
			int len = strlen(texName);
			printf("ConvertMesh: DiffuseMap: %s\n", texName);
			pIonFbxMaterial->m_pDiffuseMap = ion::Texture::Create( texName );
			}
			}
			}
			*/
		}
	}

	AABB::AABB(vec4 min, vec4 max)
	{
		aabbMin = min;
		aabbMax = max;
	}

	AABB::AABB()
	{

	}

	void AABB::Reset(float x, float y, float z)
	{
		aabbMin = (vec4){ x, y, z, 1.0f };
		aabbMax = (vec4){ x, y, z, 1.0f };
	}

	void AABB::Reset(vec4 initialPoint)
	{
		aabbMin = initialPoint;
		aabbMax = initialPoint;
	}

	void AABB::Add(vec4 p)
	{
		aabbMin.x = p.x < aabbMin.x ? p.x : aabbMin.x; // aabbMin.xmin(aabbMin, aabbMin, p);
		aabbMin.y = p.y < aabbMin.y ? p.y : aabbMin.y;
		aabbMin.z = p.z < aabbMin.z ? p.z : aabbMin.z;
		aabbMax.x = p.x > aabbMax.x ? p.x : aabbMax.x; // aabbMax.xmin(aabbMax, aabbMax, p);
		aabbMax.y = p.y > aabbMax.y ? p.y : aabbMax.y;
		aabbMax.z = p.z > aabbMax.z ? p.z : aabbMax.z;
	}

	void AABB::Print()
	{
		printf("min = %.3f, %.3f, %.3f\tmax = %.3f, %.3f, %.3f\n", aabbMin.x, aabbMin.y, aabbMin.z, aabbMax.x, aabbMax.y, aabbMax.z);
	}

#define BUFFER_OFFSET(i) ((char *)NULL + (i))
/*
    void IonFbxMesh::Render()
    {	
        glPushMatrix();
		glMultMatrixf((float*)&m_worldMatrix);
				
		//Iterate over renderables
		u32 i = m_numRenderables;
		ion::Renderable* pRenderable = m_renderables;
		while(i--)
		{			
			ion::Material* pMaterial = pRenderable->GetMaterial();
			pMaterial->SetCurrent();
			
			ion::GKernel::BindVertexBuffer(pRenderable->GetVertexBuffer());
			ion::GKernel::BindIndexBuffer(pRenderable->GetIndexBuffer());

			//Set Upper and Lower bounds for Vertex Buffer
			uint32_t lowerIndex = 0;
			uint32_t upperIndex = pRenderable->GetNumVertices();
			
			//NOTE: Using glDrawRangeElements may allow driver to optimise transfer by using 16bit indices
			ion::GKernel::DrawRangeElements(ion::kTriangles, lowerIndex, upperIndex, pRenderable->GetNumIndices(), ion::kUnsignedInt);
			++pRenderable;
		}
		        
		for (uint32_t i = 0; i < m_numChildren; ++i)
		{
			m_children[i].Render();
		}
		
		glPopMatrix();        
    }
*/

	VertexBuffer::VertexBuffer()
		: reng::Import::VertexBuffer()
		, m_numVertices(0)
		, m_data(nullptr)
	{

	}

	VertexBuffer::~VertexBuffer()
	{

	}

	void VertexBuffer::setNumStreams(uint32_t numStreams)
	{
		m_numStreams = numStreams;
	}

	void VertexBuffer::setNumVertices(uint32_t numVertices)
	{
		m_numVertices = numVertices;

		uint32_t dataSize = 0;
		for (uint32_t stream = 0; stream < m_numStreams; ++stream)
			dataSize += m_streams[stream].m_stride;
		m_data = new uint8_t [dataSize * numVertices];
	}

	void VertexBuffer::addElement(uint32_t stream, reng::Import::VertexElement& elem)
	{
		reng::Import::VertexStream* buffer = &m_streams[stream];
		uint32_t index = buffer->m_numElements++;
		buffer->m_elements[index].m_index = elem.m_index;
		buffer->m_elements[index].m_size = elem.m_size;
		buffer->m_elements[index].m_type = elem.m_type;
		buffer->m_elements[index].m_normalized = elem.m_normalized;
		buffer->m_elements[index].m_offset = elem.m_offset;
		uint16_t elementSize = 0;
		switch (elem.m_type)
		{
		case kFloat:
			elementSize = sizeof(float) * elem.m_size;
			break;
		case kHalf: 
			elementSize = sizeof(uint16_t) * elem.m_size;
			break;
		case kInt2_10_10_10_Rev:
			elementSize = sizeof(uint32_t);
			break;
		case kSignedInt:
			elementSize = sizeof(uint32_t) * elem.m_size;
			break;
		default:
			AssertMsg(false, "Unsupported element type.\n");
		}
		buffer->m_stride += elementSize;	//CLR: Todo - Do a proper size calculation based on type.
	}

	void PackVector10_10_10(uint8_t* dstPtr, uint8_t* srcPtr, uint32_t srcStride)
	{
		uint32_t *packedPtr = reinterpret_cast<uint32_t *>(dstPtr);
		*packedPtr = 0;
		float *floatPtr = reinterpret_cast<float *>(srcPtr);
		float x = floatPtr[0];
		float y = floatPtr[1];
		float z = floatPtr[2];
		AssertMsg((fabsf(x) <= 1.0f), "X component is greater than 1.0f\n");
		AssertMsg((fabsf(y) <= 1.0f), "Y component is greater than 1.0f\n");
		AssertMsg((fabsf(z) <= 1.0f), "Z component is greater than 1.0f\n");
		int32_t xInt = static_cast<int32_t>(x * (powf(2.0f, 10.0f - 1.0f) - 1.0f));
		int32_t yInt = static_cast<int32_t>(y * (powf(2.0f, 10.0f - 1.0f) - 1.0f));
		int32_t zInt = static_cast<int32_t>(z * (powf(2.0f, 10.0f - 1.0f) - 1.0f));
		*packedPtr = (zInt & 0x3ff) << 20 | (yInt & 0x3ff) << 10 | (xInt & 0x3ff);
//		*packedPtr = (xInt & 0x3ff) << 20 | (yInt & 0x3ff) << 10 | (zInt & 0x3ff);
	}

	void VertexBuffer::setData(uint32_t stream, reng::Import::VertexElement& elem, /*const*/ void* data, uint32_t srcStride)
	{
		reng::Import::VertexStream* buffer = &m_streams[stream];
		uint32_t index = elem.m_index;

		//		printf("WriteData: element %d offset %d\n", index, buffer->m_elements[index].m_offset);
		uint8_t* srcPtr = (uint8_t*)data;
		uint8_t* dstPtr = m_data + buffer->m_elements[index].m_offset;
		uint32_t dstStride = buffer->m_stride;
		for (uint32_t i = 0; i < m_numVertices; ++i)
		{
			switch (elem.m_type)
			{
			case kSignedInt:
			case kUnsignedInt:
			case kFloat:
				memcpy(dstPtr, srcPtr, srcStride);
				break;
			case kHalf:
				break;
			case kInt2_10_10_10_Rev:
				PackVector10_10_10(dstPtr, srcPtr, srcStride);
				break;
			}
			srcPtr += srcStride;
			dstPtr += dstStride;
		}
	}

	void VertexBuffer::setData(uint32_t stream, void* data)
	{
//		ion::Import::VertexStream* buffer = &m_streams[stream];
//		memcpy(buffer->m_data, data, m_numVertices*buffer->m_stride);
	}

}

