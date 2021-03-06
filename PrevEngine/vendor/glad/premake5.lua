project "glad"
    kind "StaticLib"
    language "C"
    
	targetdir ("bin/" .. outputDir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputDir .. "/%{prj.name}")

	files {
        "include/glad/glad.h",
        "include/glad/glad_wgl.h",
        "include/KHR/khrplatform.h",
		"source/glad.c",
		"source/glad_wgl.c"
    }
    
	includedirs {
		"include"
	}
	
	filter "system:windows"
        systemversion "latest"
        staticruntime "on"
		
	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"
	
	filter "configurations:Release"
		runtime "Release"
		optimize "on"
	
	filter "configurations:Distribute"
		runtime "Release"
		optimize "on"
