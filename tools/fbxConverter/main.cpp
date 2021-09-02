//
//  main.cpp
//  ionFbxConverter
//
//  Created by Claire Rogers on 08/10/2013.
//  Copyright (c) 2013 Guilty Dog Productions. All rights reserved.
//

#include "stdafx.h"
#include <string>
#include <vector>
#include <map>

//#include "ion/System/File.h"

#define USE_FBXSDK
#include "fbxConverter.h"

const uint32_t kMemMgrSize = 128 * 1024 * 1024;
const uint32_t kMemMgrAlign = 1024 * 1024;

int main(int argc, const char * argv[])
{
//	uint8_t *memoryBlock = static_cast<uint8_t *>(_aligned_malloc(kMemMgrSize, kMemMgrAlign));
//	LinearAllocator allocator(memoryBlock, kMemMgrSize);
//	ion::MemMgr::Create(allocator, kMemMgrSize, kMemMgrAlign);

#ifdef __APPLE__
	ion::SetCwd( "/Users/Claire/Development/Projects/ionEngine/assets/" );
//	ion::SetCwd( "/Users/Claire/Development/Resources/" );
#endif
	std::string inputFile;
	std::string outputFile;

	const char *cmd = nullptr;
	while ((cmd = *(argv++)) != nullptr)
	{ 
		if (cmd[0] == '-')
		{
			switch (cmd[1])
			{
			case 'i':
				inputFile = *(argv);
				break;
			case 'o':
				outputFile = *(argv);
				break;
			}
		}
	}
	if (inputFile.length() > 0 && outputFile.length() > 0)
	{
		rengineTools::FbxConverter *fbxConverter = new rengineTools::FbxConverter();
		fbxConverter->ConvertFile(inputFile, outputFile);
		delete fbxConverter;
	}
	else 
	{
		if (inputFile.length() <= 0)
		{
			printf("No input file specified.\n");
			exit(1);
		}
		if (outputFile.length() <= 0)
		{
			printf("No output file specified.\n");
			exit(1);
		}
	}
//	ion::MemMgr::Allocator().newObject<ionTools::FbxConverter>();
//	ionTools::FbxConverter::Instance().ConvertFile("TempGround.fbx", "TempGround.s3d");
//	ionTools::FbxConverter::Instance().ConvertFile("Donut2.fbx", "Donut2.s3d");
//	ionTools::FbxConverter::Instance().ConvertFile("Sphere.fbx", "Sphere.s3d");
//	ionTools::FbxConverter::Instance().ConvertFile("box.fbx", "box.s3d");
//	ionTools::FbxConverter::Instance().ConvertFile("AHRoom.fbx", "AHRoom.s3d");
//	ionTools::FbxConverter::Instance().ConvertFile("AHTable.fbx", "AHTable.s3d");
//	ionTools::FbxConverter::Instance().ConvertFile("RedBlock2.fbx", "RedBlock2.s3d");
//	ionTools::FbxConverter::Instance().ConvertFile("FloorTile.fbx", "FloorTile.s3d");
//	ionTools::FbxConverter::Instance().ConvertFile("AHTable_CL.fbx", "AHTable_CL.s3d");
//	ionTools::FbxConverter::Instance().ConvertFile("AHRoom_CL.fbx", "AHRoom_CL.s3d");
//	ionTools::IonFbxConverter::Get()->ConvertFile("BluePaddle.fbx", "BluePaddle.s3d");
//	ionTools::IonFbxConverter::Get()->ConvertFile("RedPaddle.fbx", "RedPaddle.s3d");
//	ionTools::IonFbxConverter::Get()->ConvertFile("CL_puck.fbx", "CL_puck.s3d");
	
//	ionTools::IonFbxConverter::Get()->ConvertFile("MyShip3.fbx", "MyShip3.s3d");
//	ionTools::IonFbxConverter::Get()->ConvertFile("UpperArm.fbx", "UpperArm.s3d");
//	ionTools::IonFbxConverter::Get()->ConvertFile("LowerArm.fbx", "LowerArm.s3d");
	//	ionTools::IonFbxConverter::Get()->ConvertFile("Hand.fbx", "Hand.s3d");
//	ionTools::IonFbxConverter::Get()->ConvertFile("box.fbx", "box.s3d");
//	ionTools::IonFbxConverter::Get()->ConvertFile("PhyreBot.fbx", "PhyreBot.s3d");
//	ionTools::IonFbxConverter::Get()->ConvertFile("Dalek.fbx", "Dalek.s3d");
//	ionTools::IonFbxConverter::Get()->ConvertFile("Donut.fbx", "Donut.s3d");
//	ionTools::FbxConverter::Get()->ConvertFile("Donut2.fbx", "Donut2.s3d");
//	ionTools::IonFbxConverter::Get()->ConvertFile("Sphere.fbx", "Sphere.s3d");

//	ionTools::IonFbxConverter::Get()->ConvertFile("Inn.fbx", "Inn.s3d");
//	ionTools::IonFbxConverter::Get()->ConvertFile("Cottage1.fbx", "Cottage1.s3d");
//	ionTools::IonFbxConverter::Get()->ConvertFile("Windmill.fbx", "Windmill.s3d");
//	ionTools::IonFbxConverter::Get()->ConvertFile("Church.fbx", "Church.s3d");

    return 0;
}

