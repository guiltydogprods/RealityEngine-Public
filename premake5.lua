-- premake5.lua

require "premake-modules/android/android"

newoption
{
   trigger     = "oculus",
   description = "Use Oculus."
}
	
workspace "RealityEngine"
	configurations { "Debug", "Release" }
	platforms { "x64", "ARM64" }   
	includedirs { "$(RE_SDK_PATH)" }
	defines { "_CRT_SECURE_NO_WARNINGS" }
	startproject "launcher"	
   
filter "platforms:x64"
	system "windows"
	architecture "x86_64"  
	vectorextensions "SSE4.1"
	toolset "msc-clangcl"
    defines("RE_PLATFORM_WIN64")    
	includedirs { "$(RE_SDK_PATH)/external/glfw-3.3.2/include", "$(RE_SDK_PATH)/external/glew-2.1.0/include", "$(RE_SDK_PATH)/external/fmod/api/core/inc", "$(RE_SDK_PATH)/external/ovr_spatializer_fmod_28.0.0/Include", "$(RE_SDK_PATH)/external/stb" }

filter "platforms:ARM64"
    system "android"
    architecture "arm64"
    vectorextensions "NEON"
	toolchainversion "5.0"
    defines { "__arch64__", "RE_PLATFORM_ANDROID" }    
    stl "none"
	includedirs { "$(RE_SDK_PATH)/external/fmod/api/core/inc", "$(RE_SDK_PATH)/external/ovr_spatializer_fmod_28.0.0/Include", "$(RE_SDK_PATH)/external/stb" }
	libdirs { "$(RE_SDK_PATH)/external/fmod/api/core/lib/arm64-v8a" }  
	links { "GLESv3", "EGL", "m", "log", "fmodL", "vrapi" }	

filter "configurations:Debug"
	defines { "_DEBUG" }

filter "configurations:Release"
	defines { "NDEBUG" }

project "engine"
	location "engine"
	language "C"	
	warnings "Default" --Extra"	
	buildoptions { "-Xclang -flto-visibility-public-std -fblocks" }
	compileas "C"	
	targetdir "bin/%{cfg.buildcfg}"
	if _ACTION == "vs2019" then
--		filter "platforms:x64"
--			buildoptions { "/WX", "-march=native" }		
	
--		pchheader "stdafx.h"
--		pchsource "engine/stdafx.c"
	else
--		pchheader "stdafx.h"		
--		pchsource "engine/stdafx.c"
--		buildoptions { "-x c-header", "-std=c11" }		
	end

	includedirs { "engine", "$(RE_SDK_PATH)/external/BlocksRuntime/inc" }
   	
   	filter {}

	files 
	{
		"engine/**.c", 
		"engine/**.h",
		"shaders/**.comp",
		"shaders/**.vert",
		"shaders/**.frag",
	}

	excludes
	{
		"engine/obj/**",
		"engine/internal/rpmalloc/*",
		"engine/rgfx/GLES3.2/*",
		"engine/rgfx/GL4.6/*",
--		"engine/ktx.*",
--		"engine/dds.*",
		"engine/murmur_hash3.*",
		"engine/rvrs_ovr_mobile/*",
		"engine/rvrs_ovr_win/*",
		"engine/rvrs_ovr_platform/*",
		"engine/platform/**",	
		"shaders/*_skinned.vert",		
		"shaders/*_emissive.frag",		
	}

	filter "configurations:Debug"
		defines { "_DEBUG" }
		optimize "Off"
		symbols  "Full"      
		libdirs { "lib/Debug" }

	filter "configurations:Release"
		defines { "NDEBUG" }
		optimize "Full"
		libdirs { "lib/Release" }

	filter "platforms:x64"
		kind "StaticLib"	
		excludes
		{
			"engine/pch.h"
		}

		files
		{
--			"engine/rgfx/GL4.6/*",
			"engine/rgfx/GL4.6/dds.*",
			"engine/rgfx/GL4.6/renderer_gl.h",
			"engine/rvrs_ovr_win/*",
			"engine/rvrs_ovr_platform/*",
--			"engine/dds.*",			
			"engine/platform/windows/**.c",			
			"engine/platform/windows/**.h",			
		}

		buildoptions { "-march=native -fblocks" }
		pchheader "stdafx.h"
		pchsource "engine/stdafx.c"		
	  	if _OPTIONS["oculus"] then
			includedirs { "$(RE_SDK_PATH)/external/ovr_sdk_win_23.0.0/LibOVR/Include", "$(RE_SDK_PATH)/external/ovr_platform_sdk_31.1.0/Include", "$(RE_SDK_PATH)/external/ovr_avatar_sdk_20.1/OVRAvatarSDK/Include" }
			libdirs { "$(RE_SDK_PATH)/external/ovr_sdk_win_23.0.0/LibOVR/Lib/Windows/x64/Release/VS2017", "$(RE_SDK_PATH)/external/ovr_platform_sdk_31.1.0/Windows", "$(RE_SDK_PATH)/external/ovr_avatar_sdk_20.1/OVRAvatarSDK/Windows" }			
			defines { "VRSYSTEM_OCULUS_WIN" }
		else
			defines { "VRSYSTEM_NONE" }   	
		end

	filter "platforms:ARM64"
		kind "SharedLib"	
	    androidapilevel(26)
		files
		{
--			"engine/rgfx/GLES3.2/*",
			"engine/rgfx/GLES3.2/ktx.*",
			"engine/rgfx/GLES3.2/renderer_gles.h",
			"engine/rvrs_ovr_mobile/*",
			"engine/rvrs_ovr_platform/rvrs_platform_ovr.c",			
			"engine/rvrs_ovr_platform/rvrs_avatar_ovr.c",			
--			"engine/ktx.*",
			"engine/platform/android/**.c",			
			"engine/platform/android/**.h",			
		}

		buildoptions { "-std=c11" }
		pchheader "pch.h"
		pchcompileas "C"				
	  	if _OPTIONS["oculus"] then
			includedirs { "$(RE_SDK_PATH)/external/ovr_sdk_mobile_1.45.0/VrApi/Include" }
			libdirs { "$(RE_SDK_PATH)/external/ovr_sdk_mobile_1.45.0/VrApi/Libs/Android/arm64-v8a/Release" }

			includedirs { "$(RE_SDK_PATH)/external/ovr_platform_sdk_31.1.0/Include", "$(RE_SDK_PATH)/external/ovr_avatar_sdk_20.1/OVRAvatarSDK/Include" }
			libdirs { "$(RE_SDK_PATH)/external/ovr_platform_sdk_31.1.0/Android/libs/arm64-v8a", "$(RE_SDK_PATH)/external/ovr_avatar_sdk_20.1/OVRAvatarSDK/Android/libs/arm64-v8a" }			

			defines { "VRSYSTEM_OCULUS_MOBILE" }
			links { "vrapi" }
		else
			defines { "VRSYSTEM_NONE" }   	
		end
		libdirs { "$(RE_SDK_PATH)/external/BlocksRuntime/lib/arm64-v8a/Release" }			
		links { "GLESv3", "EGL", "m", "log", "fmodL", "vrapi", "ovrplatformloader", "ovravatarloader", "BlocksRuntime" }	

project "launcher"
	removeplatforms { "ARM64" }
	location "launcher"
   	dependson { "game", "engine" }	
	language "C"	
	warnings "Default" --Extra"	
	buildoptions { "-Xclang -flto-visibility-public-std -fblocks" }
	compileas "C"	
	targetdir "bin/%{cfg.buildcfg}"
	debugdir "."
	debugargs  { '-game=bin/%{cfg.buildcfg}/game.dll' } --CLR - cfg.buildtarget.basename is incorrect so using linktarget  

	if _ACTION == "vs2019" then
--		filter "platforms:x64"
--			buildoptions { "/WX", "-march=native" }		
	
--		pchheader "stdafx.h"
--		pchsource "engine/stdafx.c"
	else
--		pchheader "stdafx.h"		
--		pchsource "engine/stdafx.c"
--		buildoptions { "-x c-header", "-std=c11" }		
	end
   	
   	filter {}

   	files
   	{
		"launcher/**.c",
		"launcher/**.h",   		
   	}

	filter "configurations:Debug"
		defines { "_DEBUG" }
		optimize "Off"
		symbols  "Full"      
		libdirs { "lib/Debug", "$(RE_SDK_PATH)/external/BlocksRuntime/lib/x64/Debug" }

	filter "configurations:Release"
		defines { "NDEBUG" }
		optimize "Full"
		libdirs { "lib/Release", "$(RE_SDK_PATH)/external/BlocksRuntime/lib/x64/Release" }

	filter "platforms:x64"
		kind "ConsoleApp"	
		buildoptions { "-march=native" }
		pchheader "stdafx.h"
		pchsource "launcher/stdafx.c"		
	  	if _OPTIONS["oculus"] then
			includedirs { "$(RE_SDK_PATH)/external/ovr_sdk_win_23.0.0/LibOVR/Include", "$(RE_SDK_PATH)/external/ovr_platform_sdk_31.1.0/Include", "$(RE_SDK_PATH)/external/ovr_avatar_sdk_20.1/OVRAvatarSDK/Include" }
			libdirs { "$(RE_SDK_PATH)/external/ovr_sdk_win_23.0.0/LibOVR/Lib/Windows/x64/Release/VS2017", "$(RE_SDK_PATH)/external/ovr_platform_sdk_31.1.0/Windows", "$(RE_SDK_PATH)/external/ovr_avatar_sdk_20.1/OVRAvatarSDK/Windows" }			
			defines { "VRSYSTEM_OCULUS_WIN" }
			links { "LibOVR.lib", "LibOVRPlatform64_1.lib", "libovravatar.lib" }
		else
			defines { "VRSYSTEM_NONE" }   	
		end
		includedirs { "$(RE_SDK_PATH)/engine", "$(RE_SDK_PATH)/external/glfw-3.3.2/include", "$(RE_SDK_PATH)/external/glew-2.1.0/include", "$(RE_SDK_PATH)/external/fmod/api/core/inc", "$(RE_SDK_PATH)/external/stb" }
		includedirs { "$(RE_SDK_PATH)/external/BlocksRuntime/inc" }
		libdirs { "$(RE_SDK_PATH)/external/glfw-3.3.2/lib-vc2019", "$(RE_SDK_PATH)/external/glew-2.1.0/lib/Release/x64", "$(RE_SDK_PATH)/external/fmod/api/core/lib/x64" }  
		links { "glfw3dll", "opengl32", "glew32", "fmodL_vc", "engine", "BlocksRuntime" }

project "game"
   location "game"
   kind "SharedLib"
   language "C"
   targetdir "bin/%{cfg.buildcfg}"   
   debugdir "."
   dependson { "../engine" }
   buildoptions { "-Xclang -flto-visibility-public-std -fblocks" }
   flags { "NoIncrementalLink" }
   editandcontinue "Off"
   symbolspath '$(OutDir)$(TargetName)-$([System.DateTime]::Now.ToString("HH_mm_ss_fff")).pdb'  
--	architecture "x86_64"  
--	vectorextensions "SSE4.1"
--	if _ACTION == "vs2019" then
--	   buildoptions { "/FS" }
--		pchheader "stdafx.h"
--		pchsource "game/stdafx.c"
--	else
--		pchheader "game/stdafx.h"		
--	end
	includedirs { "$(RE_SDK_PATH)/engine" }
	
    files 
    { 
       "game/**.h", 
       "game/**.c",
    }

    excludes
   	{
   	}

	filter "platforms:x64"
		buildoptions { "-march=native" }
		pchheader "stdafx.h"
		pchsource "game/stdafx.c"	
		links { "BlocksRuntime" }	
	  	if _OPTIONS["oculus"] then
			includedirs { "$(RE_SDK_PATH)/external/ovr_sdk_win_23.0.0/LibOVR/Include" }
			defines { "VRSYSTEM_OCULUS_WIN" }
			libdirs { "$(RE_SDK_PATH)/external/ovr_sdk_win_23.0.0/LibOVR/Lib/Windows/x64/Release/VS2017" } --CLR - how did I get a debug version again???
			links { "LibOVR.lib" }
		else
			defines { "VRSYSTEM_NONE" }   	
		end

	filter { "platforms:x64", "configurations:Debug" }
		libdirs { "$(RE_SDK_PATH)/external/BlocksRuntime/lib/x64/Debug" }	

	filter { "platforms:x64", "configurations:Release" }
		libdirs { "$(RE_SDK_PATH)/external/BlocksRuntime/lib/x64/Release" }	

	filter "platforms:ARM64"
	    androidapilevel(26)
		buildoptions { "-std=c11" }
		pchheader "pch.h"
		pchcompileas "C"					    
		links { "BlocksRuntime" }	
	  	if _OPTIONS["oculus"] then
			includedirs { "$(RE_SDK_PATH)/external/ovr_sdk_mobile_1.45.0/VrApi/Include" }
			defines { "VRSYSTEM_OCULUS_MOBILE" }
			libdirs { "$(RE_SDK_PATH)/external/ovr_sdk_mobile_1.45.0/VrApi/Libs/Android/arm64-v8a/Release" } --CLR - how did I get a debug version again???
			links { "vrapi" }
		else
			defines { "VRSYSTEM_NONE" }   	
		end

	filter { "platforms:ARM64", "configurations:Debug" }
		libdirs { "$(RE_SDK_PATH)/external/BlocksRuntime/lib/arm64-v8a/Debug" }	
	filter { "platforms:ARM64", "configurations:Release" }
		libdirs { "$(RE_SDK_PATH)/external/BlocksRuntime/lib/arm64-v8a/Release" }	

	filter "configurations:Debug"
		defines { "_DEBUG" }
		optimize "Off"
		symbols  "Full"      
		libdirs { "lib/Debug" }

	filter "configurations:Release"
		defines { "NDEBUG" }
		optimize "Full"
		libdirs { "lib/Release", "$(RE_SDK_PATH)/external/BlocksRuntime/lib/%{archname}/Release" }

project ("REQuest")
	removeplatforms { "x64" }
    location "REQuest"   
    kind("Packaging")
    androidapplibname("engine")

    files
    {
        "REQuest/AndroidManifest.xml",
        "REQuest/assets/**",
        "REQuest/build.xml",
        "REQuest/project.properties",
        "REQuest/res/values/**.xml",
        "REQuest/res/xml/**.xml",
        "REQuest/src/**.java",
        "REQuest/libs/**.so",
        "REQuest/libs/**.jar",
    }

    excludes
    {
		"REQuest/assets/shaders/*_skinned.vert",		
		"REQuest/assets/shaders/*_emissive.frag",	
		"REQuest/assets/sounds/**.mp3"	
    }

    links { "engine", "game" }
