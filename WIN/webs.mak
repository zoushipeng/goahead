# Microsoft Developer Studio Generated NMAKE File, Based on webs.dsp
!IF "$(CFG)" == ""
CFG=webs - Win32 Debug
!MESSAGE No configuration specified. Defaulting to webs - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "webs - Win32 Release" && "$(CFG)" != "webs - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
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
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "webs - Win32 Release"

OUTDIR=.
INTDIR=.
# Begin Custom Macros
OutDir=.
# End Custom Macros

ALL : "$(OUTDIR)\webs.exe"


CLEAN :
	-@erase "$(INTDIR)\asp.obj"
	-@erase "$(INTDIR)\balloc.obj"
	-@erase "$(INTDIR)\base64.obj"
	-@erase "$(INTDIR)\cgi.obj"
	-@erase "$(INTDIR)\default.obj"
	-@erase "$(INTDIR)\ejlex.obj"
	-@erase "$(INTDIR)\ejparse.obj"
	-@erase "$(INTDIR)\emfdb.obj"
	-@erase "$(INTDIR)\form.obj"
	-@erase "$(INTDIR)\h.obj"
	-@erase "$(INTDIR)\handler.obj"
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\md5c.obj"
	-@erase "$(INTDIR)\mime.obj"
	-@erase "$(INTDIR)\misc.obj"
	-@erase "$(INTDIR)\page.obj"
	-@erase "$(INTDIR)\ringq.obj"
	-@erase "$(INTDIR)\rom.obj"
	-@erase "$(INTDIR)\security.obj"
	-@erase "$(INTDIR)\sock.obj"
	-@erase "$(INTDIR)\sockGen.obj"
	-@erase "$(INTDIR)\sym.obj"
	-@erase "$(INTDIR)\uemf.obj"
	-@erase "$(INTDIR)\um.obj"
	-@erase "$(INTDIR)\umui.obj"
	-@erase "$(INTDIR)\url.obj"
	-@erase "$(INTDIR)\value.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\webrom.obj"
	-@erase "$(INTDIR)\webs.obj"
	-@erase "$(INTDIR)\websda.obj"
	-@erase "$(INTDIR)\websuemf.obj"
	-@erase "$(OUTDIR)\webs.exe"
	-@erase "$(OUTDIR)\webs.map"

CPP=cl.exe
CPP_PROJ=/nologo /Zp1 /ML /W3 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "WEBS" /D "UEMF" /D "WIN" /D "DIGEST_ACCESS_SUPPORT" /D "USER_MANAGEMENT_SUPPORT" /Fp"$(INTDIR)\webs.pch" /YX /FD /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32 
RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\webs.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=wsock32.lib kernel32.lib user32.lib /nologo /subsystem:windows /incremental:no /pdb:"$(OUTDIR)\webs.pdb" /map:"$(INTDIR)\webs.map" /machine:I386 /out:"$(OUTDIR)\webs.exe" 
LINK32_OBJS= \
	"$(INTDIR)\asp.obj" \
	"$(INTDIR)\balloc.obj" \
	"$(INTDIR)\base64.obj" \
	"$(INTDIR)\cgi.obj" \
	"$(INTDIR)\default.obj" \
	"$(INTDIR)\ejlex.obj" \
	"$(INTDIR)\ejparse.obj" \
	"$(INTDIR)\emfdb.obj" \
	"$(INTDIR)\form.obj" \
	"$(INTDIR)\h.obj" \
	"$(INTDIR)\handler.obj" \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\md5c.obj" \
	"$(INTDIR)\mime.obj" \
	"$(INTDIR)\misc.obj" \
	"$(INTDIR)\page.obj" \
	"$(INTDIR)\ringq.obj" \
	"$(INTDIR)\rom.obj" \
	"$(INTDIR)\security.obj" \
	"$(INTDIR)\sock.obj" \
	"$(INTDIR)\sockGen.obj" \
	"$(INTDIR)\sym.obj" \
	"$(INTDIR)\uemf.obj" \
	"$(INTDIR)\um.obj" \
	"$(INTDIR)\umui.obj" \
	"$(INTDIR)\url.obj" \
	"$(INTDIR)\value.obj" \
	"$(INTDIR)\webrom.obj" \
	"$(INTDIR)\webs.obj" \
	"$(INTDIR)\websda.obj" \
	"$(INTDIR)\websuemf.obj"

"$(OUTDIR)\webs.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "webs - Win32 Debug"

OUTDIR=.
INTDIR=.
# Begin Custom Macros
OutDir=.
# End Custom Macros

ALL : "$(OUTDIR)\webs.exe"


CLEAN :
	-@erase "$(INTDIR)\asp.obj"
	-@erase "$(INTDIR)\balloc.obj"
	-@erase "$(INTDIR)\base64.obj"
	-@erase "$(INTDIR)\cgi.obj"
	-@erase "$(INTDIR)\default.obj"
	-@erase "$(INTDIR)\ejlex.obj"
	-@erase "$(INTDIR)\ejparse.obj"
	-@erase "$(INTDIR)\emfdb.obj"
	-@erase "$(INTDIR)\form.obj"
	-@erase "$(INTDIR)\h.obj"
	-@erase "$(INTDIR)\handler.obj"
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\md5c.obj"
	-@erase "$(INTDIR)\mime.obj"
	-@erase "$(INTDIR)\misc.obj"
	-@erase "$(INTDIR)\page.obj"
	-@erase "$(INTDIR)\ringq.obj"
	-@erase "$(INTDIR)\rom.obj"
	-@erase "$(INTDIR)\security.obj"
	-@erase "$(INTDIR)\sock.obj"
	-@erase "$(INTDIR)\sockGen.obj"
	-@erase "$(INTDIR)\sym.obj"
	-@erase "$(INTDIR)\uemf.obj"
	-@erase "$(INTDIR)\um.obj"
	-@erase "$(INTDIR)\umui.obj"
	-@erase "$(INTDIR)\url.obj"
	-@erase "$(INTDIR)\value.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\webrom.obj"
	-@erase "$(INTDIR)\webs.obj"
	-@erase "$(INTDIR)\websda.obj"
	-@erase "$(INTDIR)\websuemf.obj"
	-@erase "$(OUTDIR)\webs.exe"
	-@erase "$(OUTDIR)\webs.map"
	-@erase "$(OUTDIR)\webs.pdb"

CPP=cl.exe
CPP_PROJ=/nologo /Zp4 /MLd /W3 /Gm /Zi /O1 /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "WIN" /D "WEBS" /D "UEMF" /D "DIGEST_ACCESS_SUPPORT" /D "USER_MANAGEMENT_SUPPORT" /D "ASSERT" /D "DEV" /FD /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32 
RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\webs.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=wsock32.lib kernel32.lib user32.lib advapi32.lib /nologo /subsystem:windows /incremental:no /pdb:"$(OUTDIR)\webs.pdb" /map:"$(INTDIR)\webs.map" /debug /machine:I386 /out:"$(OUTDIR)\webs.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\asp.obj" \
	"$(INTDIR)\balloc.obj" \
	"$(INTDIR)\base64.obj" \
	"$(INTDIR)\cgi.obj" \
	"$(INTDIR)\default.obj" \
	"$(INTDIR)\ejlex.obj" \
	"$(INTDIR)\ejparse.obj" \
	"$(INTDIR)\emfdb.obj" \
	"$(INTDIR)\form.obj" \
	"$(INTDIR)\h.obj" \
	"$(INTDIR)\handler.obj" \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\md5c.obj" \
	"$(INTDIR)\mime.obj" \
	"$(INTDIR)\misc.obj" \
	"$(INTDIR)\page.obj" \
	"$(INTDIR)\ringq.obj" \
	"$(INTDIR)\rom.obj" \
	"$(INTDIR)\security.obj" \
	"$(INTDIR)\sock.obj" \
	"$(INTDIR)\sockGen.obj" \
	"$(INTDIR)\sym.obj" \
	"$(INTDIR)\uemf.obj" \
	"$(INTDIR)\um.obj" \
	"$(INTDIR)\umui.obj" \
	"$(INTDIR)\url.obj" \
	"$(INTDIR)\value.obj" \
	"$(INTDIR)\webrom.obj" \
	"$(INTDIR)\webs.obj" \
	"$(INTDIR)\websda.obj" \
	"$(INTDIR)\websuemf.obj"

"$(OUTDIR)\webs.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("webs.dep")
!INCLUDE "webs.dep"
!ELSE 
!MESSAGE Warning: cannot find "webs.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "webs - Win32 Release" || "$(CFG)" == "webs - Win32 Debug"
SOURCE=..\asp.c

"$(INTDIR)\asp.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\balloc.c

"$(INTDIR)\balloc.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\base64.c

"$(INTDIR)\base64.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\cgi.c

"$(INTDIR)\cgi.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\default.c

"$(INTDIR)\default.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\ejlex.c

"$(INTDIR)\ejlex.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\ejparse.c

"$(INTDIR)\ejparse.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\emfdb.c

"$(INTDIR)\emfdb.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\form.c

"$(INTDIR)\form.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\h.c

"$(INTDIR)\h.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\handler.c

"$(INTDIR)\handler.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\main.c

"$(INTDIR)\main.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=..\md5c.c

"$(INTDIR)\md5c.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\mime.c

"$(INTDIR)\mime.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\misc.c

"$(INTDIR)\misc.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\page.c

"$(INTDIR)\page.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\ringq.c

"$(INTDIR)\ringq.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\rom.c

"$(INTDIR)\rom.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\security.c

"$(INTDIR)\security.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\sock.c

"$(INTDIR)\sock.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\sockGen.c

"$(INTDIR)\sockGen.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\sym.c

"$(INTDIR)\sym.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\uemf.c

"$(INTDIR)\uemf.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\um.c

"$(INTDIR)\um.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\umui.c

"$(INTDIR)\umui.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\url.c

"$(INTDIR)\url.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\value.c

"$(INTDIR)\value.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\webrom.c

"$(INTDIR)\webrom.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\webs.c

"$(INTDIR)\webs.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\websda.c

"$(INTDIR)\websda.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\websuemf.c

"$(INTDIR)\websuemf.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

