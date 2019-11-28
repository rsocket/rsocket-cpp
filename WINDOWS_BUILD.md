
## Install on Windows

This document page describes a way to build rsocket-cpp with 'Visual Studio tools for CMake' in Visual Studio 2017 on Windows. 

### Pre-Acquisition 

#### Install Vcpkg 

During the build process of rsocket-cpp, it requires [folly](https://github.com/facebook/folly) and it is available in [vcpkg](https://github.com/Microsoft/vcpkg#vcpkg). 

#### Install CMake Tools

1. Run Visual Studio 2017. 
    * *it is recommended to use higher version than 15.4.5. due to the issue that 15.4 couldn't open CMake correctly.*
2. Select *Tools > Get Tools and Features...* menu.
3. Go to **Indivisual components** tab
4. See *'Compilers, build tools, and runtimes* and check **Visual C++ tools for CMake.**
5. See *'Development activities'* and check the followings:
    * **Visual C++ for Linux Development.** *(Not sure though it is really necessary)*
    * **Visual C++ tools for CMake and Linux.** *(Not sure though it is really necessary)*
6. Press 'Modify' button!

### Open CMake Project

1. Run Visual Studio 2017
2. Select *Open > File > CMake* menu then select *'CMakeList.txt'* file under the rsocket-cpp repository directory.
3. Select *CMake > Change_CMake_Settings > CMakeList.txt* menu, and it will generate **CMakeSettings.json** file.


#### CMakeSettings 

The created CMakeSettings.json file will be opened automatically but it will be not, you have to do manually. 

The first thing you need to do is changing the directory path of **buildRoot** and **installRoot** to have its directory under the repository directory.

Change
```"buildRoot": "${env.USERPROFILE}\\CMakeBuilds\\${workspaceHash}\\build\\${name}",```
as 
```"buildRoot": "${projectDir}\\build\\${name}",```

Next, change 
``` "installRoot": "${env.USERPROFILE}\\CMakeBuilds\\${workspaceHash}\\install\\${name}", ```
as
```"installRoot": "${projectDir}\\install\\${name}",```

Now, all the files such as .obj, .lib, and .exe will be put under the changed directory in your repository directorys. 

You might also need to change **cmakeCommandArgs** argument to be set "-DBUILD_TESTS=OFF" which disables google test. (due to the issue #869)

#### Install folly 

Install [folly](https://github.com/facebook/folly) thorough [vcpkg](https://github.com/Microsoft/vcpkg#vcpkg).

Open command-line(cmd) window and move to where vcpkg.exe is and run the following:

```
.\vcpkg.exe install folly:x64-windows --head
```

After the installation, it has to be integrated to use the package in Visual Studio 2017. Run the following:
```
.\vcpkg.exe integrate install
```

To make the project can refer ***folly***, the external package, it needs to change CMakeSettings.json file. Add **variables** element and get it to have 'name' and 'value' like the following.

```json
{
  "configurations": [
    {
      "name": "x64-Debug",
      "generator": "Ninja",
      "configurationType": "Debug",
      "inheritEnvironments": [
        "msvc_x64_x64"
      ],
      "buildRoot": "${project`Dir}\\build\\${name}",
      "installRoot": "${projectDir}\\install\\${name}",
      "cmakeCommandArgs": "-DBUILD_TESTS=OFF",
      "buildCommandArgs": "-v",
      "ctestCommandArgs": "",
      "variables": [
        {
          "name": "CMAKE_TOOLCHAIN_FILE",
          "value": "C:\\dev\\gRPC\\install\\vcpkg\\scripts\\buildsystems\\vcpkg.cmake"
        }      
    }
  ]
}
```
Save the modification and you can see CMakeCache is refreshed then it refers 'folly' package correctly. 

Now you should see *Build* and other menu items under *CMake* menu but there are still a few things to be tweaked.


Open 'rsocket_repository/CMakeList.txt' file and go to line 177 and add ```if (NOT MSVC)``` so the change looks like the following:
```json
if (NOT MSVC)
	if("${BUILD_TYPE_LOWER}" MATCHES "debug")
	  message("debug mode was set")
	  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unreachable-code")
	else()
	  message("release mode was set")
	endif()
endif()
```
Without the above modification, it might meet the following error messasge when it built:
>`Error	D8021	invalid numeric argument '/Wno-unreachable-code'


Open 'rsocket_repository/rsocket/benchmarks/CMakeList.txt' file. And add ```If (NOT MSVC)``` at the first line not to build entire benchmark related files. Don't forget to add ```endif()``` at the end of the line either. Now, it should look like the following:
```json
if (NOT MSVC`)
  add_library(fixture Fixture.cpp Fixture.h)
  target_link_libraries(fixture ReactiveSocket Folly::folly)
  ...
endif()
```
Without the above modification, it might meet the following error messasge when it built:
>Error	C1083	Cannot open include file: 'arpa/inet.h': No such file or directory


### Build and Install

Now, you can go build and install all the libraries and examples. 

*Note: All the above are tested on Visual Studio 2017 v15.9.11 in Windows10*
