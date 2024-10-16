@echo off

set cl_flags=-FC -GR- -EHa- -nologo -Zi -W4 -WX -wd4201 -wd4100 -wd4098 -DSDL_MAIN_HANDLED /DDEBUG /Fm /Oi /MT /I C:\VulkanSDK\1.3.261.0\Include\

set link_flags=/nologo /incremental:no /opt:ref C:\VulkanSDK\1.3.261.0\Lib\vulkan-1.lib C:\VulkanSDK\1.3.261.0\Lib\SDL2.lib

pushd build\
cl %cl_flags% ..\source.c /link %link_flags%
popd

