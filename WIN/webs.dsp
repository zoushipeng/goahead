# Microsoft Developer Studio Project File - Name="webs" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=webs - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "webs.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "webs.mak" CFG="webs - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "webs - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "webs - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "webs - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ""
# PROP Intermediate_Dir ""
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /Zp1 /W3 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "WEBS" /D "UEMF" /D "WIN" /D "DIGEST_ACCESS_SUPPORT" /D "USER_MANAGEMENT_SUPPORT" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 wsock32.lib kernel32.lib user32.lib /nologo /subsystem:windows /map /machine:I386

!ELSEIF  "$(CFG)" == "webs - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ""
# PROP Intermediate_Dir ""
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /Zp4 /W3 /Gm /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "WIN" /D "WEBS" /D "UEMF" /D "DIGEST_ACCESS_SUPPORT" /D "USER_MANAGEMENT_SUPPORT" /D "ASSERT" /D "DEV" /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG" /d "WEBS" /d "WIN"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 wsock32.lib kernel32.lib user32.lib advapi32.lib /nologo /subsystem:windows /map /debug /machine:I386 /pdbtype:sept
# SUBTRACT LINK32 /incremental:no

!ENDIF 

# Begin Target

# Name "webs - Win32 Release"
# Name "webs - Win32 Debug"
# Begin Source File

SOURCE=..\asp.c
# End Source File
# Begin Source File

SOURCE=..\balloc.c
# End Source File
# Begin Source File

SOURCE=..\base64.c
# End Source File
# Begin Source File

SOURCE=..\cgi.c
# End Source File
# Begin Source File

SOURCE=..\default.c
# End Source File
# Begin Source File

SOURCE=..\ejlex.c
# End Source File
# Begin Source File

SOURCE=..\ejparse.c
# End Source File
# Begin Source File

SOURCE=..\emfdb.c
# End Source File
# Begin Source File

SOURCE=..\form.c
# End Source File
# Begin Source File

SOURCE=..\h.c
# End Source File
# Begin Source File

SOURCE=..\handler.c
# End Source File
# Begin Source File

SOURCE=.\main.c
# End Source File
# Begin Source File

SOURCE=..\md5c.c
# End Source File
# Begin Source File

SOURCE=..\mime.c
# End Source File
# Begin Source File

SOURCE=..\misc.c
# End Source File
# Begin Source File

SOURCE=..\page.c
# End Source File
# Begin Source File

SOURCE=..\ringq.c
# End Source File
# Begin Source File

SOURCE=..\rom.c
# End Source File
# Begin Source File

SOURCE=..\security.c
# End Source File
# Begin Source File

SOURCE=..\sock.c
# End Source File
# Begin Source File

SOURCE=..\sockGen.c
# End Source File
# Begin Source File

SOURCE=..\sym.c
# End Source File
# Begin Source File

SOURCE=..\uemf.c
# End Source File
# Begin Source File

SOURCE=..\um.c
# End Source File
# Begin Source File

SOURCE=..\umui.c
# End Source File
# Begin Source File

SOURCE=..\url.c
# End Source File
# Begin Source File

SOURCE=..\value.c
# End Source File
# Begin Source File

SOURCE=..\webrom.c
# End Source File
# Begin Source File

SOURCE=..\webs.c
# End Source File
# Begin Source File

SOURCE=..\websda.c
# End Source File
# Begin Source File

SOURCE=..\websuemf.c
# End Source File
# End Target
# End Project
