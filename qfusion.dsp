# Microsoft Developer Studio Project File - Name="qfusion" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=qfusion - WIN32 RELEASE
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "qfusion.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "qfusion.mak" CFG="qfusion - WIN32 RELEASE"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "qfusion - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "qfusion - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "qfusion - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\Release"
# PROP BASE Intermediate_Dir ".\Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\release"
# PROP Intermediate_Dir ".\release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /G5 /MT /W3 /GX /O2 /I "qcommon" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 winmm.lib wsock32.lib kernel32.lib user32.lib gdi32.lib zlibstat.lib libjpeg.lib dxguid.lib /nologo /subsystem:windows /machine:I386 /nodefaultlib:"libc" /libpath:".\win32/lib"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "qfusion - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\Debug"
# PROP BASE Intermediate_Dir ".\Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\debug"
# PROP Intermediate_Dir ".\debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /G5 /MTd /W3 /GX /ZI /Od /I "qcommon" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FR /YX /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386
# ADD LINK32 winmm.lib wsock32.lib kernel32.lib user32.lib gdi32.lib zlibstat.lib libjpeg.lib dxguid.lib /nologo /subsystem:windows /profile /map /debug /machine:I386 /nodefaultlib:"libc" /libpath:".\win32/lib"

!ENDIF 

# Begin Target

# Name "qfusion - Win32 Release"
# Name "qfusion - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\client\cin.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_cin.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_demo.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_ents.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_game.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_input.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_main.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_parse.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_screen.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_ui.c
# End Source File
# Begin Source File

SOURCE=.\qcommon\cmd.c
# End Source File
# Begin Source File

SOURCE=.\qcommon\cmodel.c
# End Source File
# Begin Source File

SOURCE=.\qcommon\common.c
# End Source File
# Begin Source File

SOURCE=.\win32\conproc.c
# End Source File
# Begin Source File

SOURCE=.\client\console.c
# End Source File
# Begin Source File

SOURCE=.\qcommon\cvar.c
# End Source File
# Begin Source File

SOURCE=.\qcommon\files.c
# End Source File
# Begin Source File

SOURCE=.\win32\glw_imp.c
# End Source File
# Begin Source File

SOURCE=.\qcommon\huff.c
# End Source File
# Begin Source File

SOURCE=.\win32\in_win.c
# End Source File
# Begin Source File

SOURCE=.\client\keys.c
# End Source File
# Begin Source File

SOURCE=.\qcommon\mdfour.c
# End Source File
# Begin Source File

SOURCE=.\qcommon\mem.c
# End Source File
# Begin Source File

SOURCE=.\qcommon\net_chan.c
# End Source File
# Begin Source File

SOURCE=.\win32\net_wins.c
# End Source File
# Begin Source File

SOURCE=.\qcommon\patch.c
# End Source File
# Begin Source File

SOURCE=.\game\q_math.c
# End Source File
# Begin Source File

SOURCE=.\game\q_shared.c
# End Source File
# Begin Source File

SOURCE=.\win32\q_shwin.c
# End Source File
# Begin Source File

SOURCE=.\win32\qgl_win.c
# End Source File
# Begin Source File

SOURCE=.\ref_gl\r_alias.c
# End Source File
# Begin Source File

SOURCE=.\ref_gl\r_backend.c
# End Source File
# Begin Source File

SOURCE=.\ref_gl\r_cin.c
# End Source File
# Begin Source File

SOURCE=.\ref_gl\r_draw.c
# End Source File
# Begin Source File

SOURCE=.\ref_gl\r_image.c
# End Source File
# Begin Source File

SOURCE=.\ref_gl\r_light.c
# End Source File
# Begin Source File

SOURCE=.\ref_gl\r_main.c
# End Source File
# Begin Source File

SOURCE=.\ref_gl\r_mesh.c
# End Source File
# Begin Source File

SOURCE=.\ref_gl\r_misc.c
# End Source File
# Begin Source File

SOURCE=.\ref_gl\r_model.c
# End Source File
# Begin Source File

SOURCE=.\ref_gl\r_poly.c
# End Source File
# Begin Source File

SOURCE=.\ref_gl\r_shader.c
# End Source File
# Begin Source File

SOURCE=.\ref_gl\r_shadow.c
# End Source File
# Begin Source File

SOURCE=.\ref_gl\r_skin.c
# End Source File
# Begin Source File

SOURCE=.\ref_gl\r_skm.c
# End Source File
# Begin Source File

SOURCE=.\ref_gl\r_surf.c
# End Source File
# Begin Source File

SOURCE=.\ref_gl\r_warp.c
# End Source File
# Begin Source File

SOURCE=.\client\snd_dma.c
# End Source File
# Begin Source File

SOURCE=.\client\snd_mem.c
# End Source File
# Begin Source File

SOURCE=.\client\snd_mix.c
# End Source File
# Begin Source File

SOURCE=.\win32\snd_win.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_ccmds.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_ents.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_game.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_init.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_main.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_send.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_user.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_world.c
# End Source File
# Begin Source File

SOURCE=.\win32\sys_win.c
# End Source File
# Begin Source File

SOURCE=.\qcommon\unzip.c
# End Source File
# Begin Source File

SOURCE=.\win32\vid_win.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=.\client\anorms.h
# End Source File
# Begin Source File

SOURCE=.\client\cin.h
# End Source File
# Begin Source File

SOURCE=.\client\client.h
# End Source File
# Begin Source File

SOURCE=.\win32\conproc.h
# End Source File
# Begin Source File

SOURCE=.\client\console.h
# End Source File
# Begin Source File

SOURCE=.\client\input.h
# End Source File
# Begin Source File

SOURCE=.\qcommon\ioapi.h
# End Source File
# Begin Source File

SOURCE=.\client\keys.h
# End Source File
# Begin Source File

SOURCE=.\win32\net_wins.h
# End Source File
# Begin Source File

SOURCE=.\game\q_shared.h
# End Source File
# Begin Source File

SOURCE=.\qcommon\qcommon.h
# End Source File
# Begin Source File

SOURCE=.\game\qfconfig.h
# End Source File
# Begin Source File

SOURCE=.\qcommon\qfiles.h
# End Source File
# Begin Source File

SOURCE=.\ref_gl\qgl.h
# End Source File
# Begin Source File

SOURCE=.\ref_gl\r_backend.h
# End Source File
# Begin Source File

SOURCE=.\ref_gl\r_local.h
# End Source File
# Begin Source File

SOURCE=.\ref_gl\r_mesh.h
# End Source File
# Begin Source File

SOURCE=.\ref_gl\r_model.h
# End Source File
# Begin Source File

SOURCE=.\ref_gl\r_shader.h
# End Source File
# Begin Source File

SOURCE=.\ref_gl\r_shadow.h
# End Source File
# Begin Source File

SOURCE=.\ref_gl\r_skin.h
# End Source File
# Begin Source File

SOURCE=.\ref_gl\render.h
# End Source File
# Begin Source File

SOURCE=.\client\screen.h
# End Source File
# Begin Source File

SOURCE=.\server\server.h
# End Source File
# Begin Source File

SOURCE=.\client\snd_loc.h
# End Source File
# Begin Source File

SOURCE=.\client\sound.h
# End Source File
# Begin Source File

SOURCE=.\qcommon\unzip.h
# End Source File
# Begin Source File

SOURCE=.\client\vid.h
# End Source File
# Begin Source File

SOURCE=.\game\win32conf.h
# End Source File
# Begin Source File

SOURCE=.\win32\winquake.h
# End Source File
# Begin Source File

SOURCE=.\qcommon\zconf.h
# End Source File
# Begin Source File

SOURCE=.\qcommon\zlib.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;cnt;rtf;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\win32\q2.ico
# End Source File
# Begin Source File

SOURCE=.\win32\qe3.ico
# End Source File
# Begin Source File

SOURCE=.\win32\qf.rc
# End Source File
# End Group
# End Target
# End Project
