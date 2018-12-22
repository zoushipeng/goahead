# Microsoft Developer Studio Generated NMAKE File, Based on webcomp.dsp
!IF "$(CFG)" == ""
CFG=webcomp - Win32 Release
!MESSAGE No configuration specified. Defaulting to "$(CFG)".
!ENDIF 

!IF "$(CFG)" != "webcomp - Win32 Release" && "$(CFG)" !=\
 "webcomp - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "webcomp.mak" CFG="webcomp - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "webcomp - Win32 Release" (based on\
 "Win32 (x86) Console Application")
!MESSAGE "webcomp - Win32 Debug" (based on\
 "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "webcomp - Win32 Release"

OUTDIR=.
INTDIR=.
# Begin Custom Macros
OutDir=.
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\webcomp.exe"

!ELSE 

ALL : "$(OUTDIR)\webcomp.exe"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\webcomp.obj"
	-@erase "$(OUTDIR)\webcomp.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /ML /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D\
 "_MBCS" /D "UEMF" /D "WEBS" /D WIN /Fp"$(INTDIR)\webcomp.pch" /YX /Fo"$(INTDIR)\\" \
 /Fd"$(INTDIR)\\" /FD /c 
CPP_OBJS=.
CPP_SBRS=.

.c{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\webcomp.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib /nologo /subsystem:console /incremental:no\
 /pdb:"$(OUTDIR)\webcomp.pdb" /machine:I386 /out:"$(OUTDIR)\webcomp.exe" 
LINK32_OBJS= \
	"$(INTDIR)\webcomp.obj"

"$(OUTDIR)\webcomp.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "webcomp - Win32 Debug"

OUTDIR=.
INTDIR=.
# Begin Custom Macros
OutDir=.
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\webcomp.exe"

!ELSE 

ALL : "$(OUTDIR)\webcomp.exe"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\vc50.pdb"
	-@erase "$(INTDIR)\webcomp.obj"
	-@erase "$(OUTDIR)\webcomp.exe"
	-@erase "$(OUTDIR)\webcomp.ilk"
	-@erase "$(OUTDIR)\webcomp.pdb"

CPP=cl.exe
CPP_PROJ=/nologo /MLd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE"\
 /D "_MBCS" /D "UEMF" /D "WEBS" /D WIN /Fp"$(INTDIR)\webcomp.pch" \
 /YX /FD /c 
CPP_OBJS=.
CPP_SBRS=.

.c{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\webcomp.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib /nologo /subsystem:console /incremental:yes\
 /pdb:"$(OUTDIR)\webcomp.pdb" /debug /machine:I386\
 /out:"$(OUTDIR)\webcomp.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\webcomp.obj"

"$(OUTDIR)\webcomp.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(CFG)" == "webcomp - Win32 Release" || "$(CFG)" ==\
 "webcomp - Win32 Debug"
SOURCE=..\webcomp.c
DEP_CPP_WEBCO=\
	"..\ej.h"\
	"..\ejIntrn.h"\
	"..\uemf.h"\
	"..\webs.h"\
	"..\wsIntrn.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	
NODEP_CPP_WEBCO=\
	"..\arpa\inet.h"\
	"..\basic\basic.h"\
	"..\emf\emf.h"\
	"..\netdb.h"\
	"..\netinet\in.h"\
	"..\sys\select.h"\
	"..\sys\socket.h"\
	

"$(INTDIR)\webcomp.obj" : $(SOURCE) $(DEP_CPP_WEBCO) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

