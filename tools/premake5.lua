-- premake5.lua
	
workspace "Tools"
	configurations { "Debug", "Release" }
	platforms { "x64" }   
--	includedirs { ".", "engine" }
	defines { "_CRT_SECURE_NO_WARNINGS" }
	startproject "fonttool"	
   
filter "platforms:x64"
	system "windows"
	architecture "x86_64"  
	vectorextensions "SSE4.1"
	toolset "msc-clangcl"
    defines("RE_PLATFORM_WIN64")    
	includedirs { "..", "../external/glfw-3.3.2/include", "../external/glew-2.1.0/include", "../external/fmod/api/core/inc", "../external/stb" }

filter "configurations:Debug"
	defines { "_DEBUG" }

filter "configurations:Release"
	defines { "NDEBUG" }

project "fontTool"
	removeplatforms { "ARM64" }
	location "fontTool"
	kind "ConsoleApp"
	language "C"
	targetdir "bin/%{cfg.buildcfg}"   	
	debugdir "."
	buildoptions { "-Xclang -flto-visibility-public-std" }
	editandcontinue "Off"
	
	files 
	{ 
		"fontTool/**.h", 
		"fontTool/**.c",
	}

	excludes
	{	
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

project "fbxConverter"
	removeplatforms { "ARM64" }
	location "fbxConverter"
	kind "ConsoleApp"
	language "C++"
--	dependson { "ion" }   
	targetdir "bin/%{cfg.buildcfg}"
	debugdir "."

--	pchheader "stdafx.h"
--	pchsource "fbxConverter/stdafx.cpp"

	files 
	{
		"fbxConverter/**.h",
		"fbxConverter/**.cpp",
	}

	buildoptions { "-Xclang -flto-visibility-public-std" }

	includedirs { "../external/fbxsdk/2020.2/include", "../engine" }

	links { "libfbxsdk-md.lib", "libxml2-md.lib", "zlib-md.lib", } --"ionGL" }
  
  	filter "configurations:Debug"
  		defines { "_DEBUG" }
  		symbols  "On"
  		libdirs { "lib/Debug", "../external/fbxsdk/2020.2/lib/vs2019/x64/debug" }

	filter "configurations:Release"
		defines { "NDEBUG" }
		optimize "On"
		libdirs { "lib/Release", "../external/fbxsdk/2020.2/lib/vs2019/x64/release" }
