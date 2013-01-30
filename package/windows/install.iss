;
; install.iss -- Inno Setup 5 install configuration file for Embedthis GoAhead
;
; Copyright (c) Embedthis Software LLC, 2003-2013. All Rights Reserved.
;

[Setup]
AppName=${settings.title}
AppVerName=${settings.title} ${settings.version}-${settings.buildNumber}
DefaultDirName={pf}\${settings.title}
DefaultGroupName=${settings.product}
UninstallDisplayIcon={app}/${settings.product}.exe
LicenseFile=LICENSE.TXT
ArchitecturesInstallIn64BitMode=x64

[Code]
var
	PortPage: TInputQueryWizardPage;
	SSLPortPage: TInputQueryWizardPage;
	WebDirPage: TInputDirWizardPage;


procedure InitializeWizard();
begin

	WebDirPage := CreateInputDirPage(wpSelectDir, 'Select Web Documents Directory', 'Where should web files be stored?',
		'Select the folder in which to store web documents, then click Next.', False, '');
	WebDirPage.Add('');
	WebDirPage.values[0] := ExpandConstant('{sd}') + '/goahead/web';

	PortPage := CreateInputQueryPage(wpSelectComponents, 'HTTP Port', 'Primary TCP/IP Listen Port for HTTP Connections',
		'Please specify the TCP/IP port on which GoAhead should listen for HTTP requests.');
	PortPage.Add('HTTP Port:', False);
	PortPage.values[0] := '80';
end;


//
//	Initial sample by Jared Breland
//
procedure AddPath(keyName: String; dir: String);
var
	newPath, oldPath, key: String;
	paths:		TArrayOfString;
	i:			Integer;
	regHive:	Integer;
begin

  if (IsAdminLoggedOn or IsPowerUserLoggedOn) then begin
    regHive := HKEY_LOCAL_MACHINE;
    key := 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment';
  end else begin
    regHive := HKEY_CURRENT_USER;
    key := 'Environment';
  end;

	i := 0;
	if RegValueExists(regHive, key, keyName) then begin
		RegQueryStringValue(regHive, key, keyName, oldPath);
		oldPath := oldPath + ';';

		while (Pos(';', oldPath) > 0) do begin
			SetArrayLength(paths, i + 1);
			paths[i] := Copy(oldPath, 0, Pos(';', oldPath) - 1);
			oldPath := Copy(oldPath, Pos(';', oldPath) + 1, Length(oldPath));
			i := i + 1;

			if dir = paths[i - 1] then begin
				continue;
			end;

			if i = 1 then begin
				newPath := paths[i - 1];
			end else begin
				newPath := newPath + ';' + paths[i - 1];
			end;
		end;
	end;

	if (IsUninstaller() = false) and (dir <> '') then begin
		if (newPath <> '') then begin
			newPath := newPath + ';' + dir;
		end else begin
	    	newPath := dir;
	  end;
  	end;
	RegWriteStringValue(regHive, key, keyName, newPath);
end;


procedure CurStepChanged(CurStep: TSetupStep);
var
  path: String;
  bin: String;
  app: String;
  rc: Integer;
begin
  if CurStep = ssInstall then
  begin
   app := ExpandConstant('{app}');
   
   path := app + '/bin/goaheadMonitor.exe';
   if FileExists(path) then
     Exec(path, '--stop', app, 0, ewWaitUntilTerminated, rc);

   path := app + '/bin/appman.exe';
   if FileExists(path) then
     Exec(path, 'stop', app, 0, ewWaitUntilTerminated, rc);
   end;
   if CurStep = ssPostInstall then
     if IsTaskSelected('addpath') then begin
       bin := ExpandConstant('{app}\bin');      
       // AddPath('EJSPATH', bin);
       AddPath('Path', bin);
    end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
	app:			String;
	bin:			String;
begin
	if CurUninstallStep = usUninstall then begin
	    bin := ExpandConstant('{app}\bin');			
		// AddPath('EJSPATH', bin);
		AddPath('Path', bin);
	end;
	if CurUninstallStep = usDone then begin
	    app := ExpandConstant('{app}');			
        RemoveDir(app);
    end;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  if (CurPageID = 6) then
  begin
    WebDirPage.values[0] := ExpandConstant('{app}') + '/web';
  end;
  Result := true;
end;

function GetWebDir(Param: String): String;
begin
  Result := WebDirPage.Values[0];
end;


function GetPort(Param: String): String;
begin
  Result := PortPage.Values[0];
end;

function GetSSL(Param: String): String;
begin
  // Result := SSLPortPage.Values[0];
  Result := '443';
end;


function IsPresent(const file: String): Boolean;
begin
  file := ExpandConstant(file);
  if FileExists(file) then begin
    Result := True;
  end else begin
    Result := False;
  end
end;


[Icons]
Name: "{group}\${settings.product}Monitor"; Filename: "{app}/bin/${settings.product}Monitor.exe";
Name: "{group}\ReadMe"; Filename: "{app}/README.TXT"

[Registry]
Root: HKLM; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "GoAheadMonitor";
ValueData: "{app}\bin\goaheadMonitor.exe"

[Dirs]
Name: "{app}/logs"; Permissions: system-modify;
Name: "{app}/bin"

[UninstallDelete]
; Type: files; Name: "{app}/route.txt";
; Type: files; Name: "{app}/auth.txt";
Type: files; Name: "{app}/logs/access.log.old";
Type: files; Name: "{app}/logs/access.log.*";
Type: files; Name: "{app}/logs/error.log";
Type: files; Name: "{app}/logs/error.log.old";
Type: files; Name: "{app}/logs/error.log.*";
Type: files; Name: "{app}/cache/*.*";
Type: filesandordirs; Name: "{app}/*.obj";

[Tasks]
Name: addpath; Description: Add ${settings.title} to the system PATH variable;

[Run]
Filename: "{app}/bin/${settings.product}Monitor.exe"; Parameters: "--stop"; WorkingDir: "{app}/bin"; Check: IsPresent('{app}/bin/${settings.product}Monitor.exe'); StatusMsg: "Stopping the GoAhead Monitor"; Flags: waituntilterminated;

Filename: "{app}/bin/appman.exe"; Parameters: "uninstall"; WorkingDir: "{app}"; Check: IsPresent('{app}/bin/appman.exe');
StatusMsg: "Stopping GoAhead"; Flags: waituntilterminated;

Filename: "{app}/bin/setConfig.exe"; Parameters: "--home . --documents ""{code:GetWebDir}"" --logs logs --port
{code:GetPort} --ssl {code:GetSSL} --cache cache --modules bin goahead.conf"; WorkingDir: "{app}"; StatusMsg: "Updating GoAhead configuration"; Flags: runhidden waituntilterminated; 

Filename: "{app}/bin/appman.exe"; Parameters: "install enable"; WorkingDir: "{app}"; StatusMsg: "Installing GoAhead as a Windows Service"; Flags: waituntilterminated;

Filename: "{app}/bin/appman.exe"; Parameters: "start"; WorkingDir: "{app}"; StatusMsg: "Starting the GoAhead Server"; Flags: waituntilterminated;

Filename: "{app}/bin/${settings.product}Monitor.exe"; Parameters: ""; WorkingDir: "{app}/bin"; StatusMsg: "Starting the
GoAhead Monitor"; Flags: waituntilidle;

Filename: "http://embedthis.com/products/goahead/doc/index.html"; Description: "View the Documentation"; Flags: skipifsilent waituntilidle shellexec postinstall;

[UninstallRun]
Filename: "{app}/bin/${settings.product}Monitor.exe"; Parameters: "--stop"; WorkingDir: "{app}"; StatusMsg: "Stopping the
GoAhead Monitor"; Flags: waituntilterminated;
Filename: "{app}/bin/appman.exe"; Parameters: "uninstall"; WorkingDir: "{app}"; Check: IsPresent('{app}/bin/appman.exe');
Filename: "{app}/bin/removeFiles.exe"; Parameters: "-r -s 5"; WorkingDir: "{app}"; Flags:

[Files]
