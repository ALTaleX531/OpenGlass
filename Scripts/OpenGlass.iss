#define MyAppName "OpenGlass"
#define MyAppVersion "2.6.2.2891"
#define MyAppPublisher "ALTaleX"
#define MyAppURL "https://github.com/ALTaleX531/OpenGlass"
#define MyAppExeName "OpenGlassGUI.exe"
#ifndef SetupName
#define SetupName "OpenGlassSetup"
#endif
#define MyAppSetupName SetupName
#define MyProjPath ".."
#ifndef BuildPath
#define BuildPath MyProjPath + "\Build\x64\Release"
#endif
#define MyAppBuildPath BuildPath
#define MyAppInstallerVersion "1.0.0.1"

[Setup]
AppId={{D3D1BC7D-5E24-4B33-9383-7934271A3B05}}
AppName={#MyAppName}
VersionInfoVersion={#MyAppInstallerVersion}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} v{#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
AppendDefaultDirName=yes
DefaultDirName={commonpf}\{#MyAppName}
DisableDirPage=auto
DirExistsWarning=auto
AlwaysShowDirOnReadyPage=yes
UsePreviousAppDir=yes
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=auto
UsePreviousGroup=yes
AllowNoIcons=yes
LicenseFile={#MyProjPath}\LICENSE.txt
OutputDir={#MyAppBuildPath}
OutputBaseFilename={#MyAppSetupName}
Compression=lzma
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Mandatory administrative privileges
PrivilegesRequired=admin
; Support Win10 1809 (Build 17763) and newer
MinVersion=10.0.17763
SetupLogging=yes
CloseApplications=yes
RestartApplications=no
CloseApplicationsFilter=OpenGlassGUI.exe,OpenGlassHost.exe
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName=OpenGlass v{#MyAppVersion}
Uninstallable=yes
UpdateUninstallLogAppName=yes
UsePreviousLanguage=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "arabic"; MessagesFile: "compiler:Languages\Arabic.isl"
Name: "armenian"; MessagesFile: "compiler:Languages\Armenian.isl"
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "bulgarian"; MessagesFile: "compiler:Languages\Bulgarian.isl"
Name: "catalan"; MessagesFile: "compiler:Languages\Catalan.isl"
#if FileExists(CompilerPath + "Languages\\ChineseSimplified.isl")
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"
#endif
#if FileExists(CompilerPath + "Languages\\ChineseTraditional.isl")
Name: "chinesestraditional"; MessagesFile: "compiler:Languages\ChineseTraditional.isl"
#endif
Name: "corsican"; MessagesFile: "compiler:Languages\Corsican.isl"
Name: "czech"; MessagesFile: "compiler:Languages\Czech.isl"
Name: "danish"; MessagesFile: "compiler:Languages\Danish.isl"
Name: "dutch"; MessagesFile: "compiler:Languages\Dutch.isl"
Name: "finnish"; MessagesFile: "compiler:Languages\Finnish.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "hebrew"; MessagesFile: "compiler:Languages\Hebrew.isl"
Name: "hungarian"; MessagesFile: "compiler:Languages\Hungarian.isl"
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"
Name: "korean"; MessagesFile: "compiler:Languages\Korean.isl"
Name: "norwegian"; MessagesFile: "compiler:Languages\Norwegian.isl"
Name: "polish"; MessagesFile: "compiler:Languages\Polish.isl"
Name: "portuguese"; MessagesFile: "compiler:Languages\Portuguese.isl"
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "slovak"; MessagesFile: "compiler:Languages\Slovak.isl"
Name: "slovenian"; MessagesFile: "compiler:Languages\Slovenian.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "swedish"; MessagesFile: "compiler:Languages\Swedish.isl"
Name: "tamil"; MessagesFile: "compiler:Languages\Tamil.isl"
Name: "turkish"; MessagesFile: "compiler:Languages\Turkish.isl"
Name: "ukrainian"; MessagesFile: "compiler:Languages\Ukrainian.isl"

[CustomMessages]
LaunchOpenGlassGUI=Launch OpenGlass GUI
DeleteUserConfig=Delete user configuration (registry settings)?
ServiceDescription=This service injects DLL into DWM for you and also maintains that user settings are correctly loaded.

#if FileExists(CompilerPath + "Languages\\ChineseSimplified.isl")
chinesesimplified.LaunchOpenGlassGUI=启动 OpenGlass GUI
chinesesimplified.DeleteUserConfig=删除用户配置（注册表设置）？
chinesesimplified.ServiceDescription=该服务会为您将 DLL 注入 DWM，并确保 OpenGlass 能正确加载用户配置。
#endif

#if FileExists(CompilerPath + "Languages\\ChineseTraditional.isl")
chinesestraditional.LaunchOpenGlassGUI=啟動 OpenGlass GUI
chinesestraditional.DeleteUserConfig=刪除使用者配置（登錄表設置）？
chinesestraditional.ServiceDescription=該服務負責為閣下將 DLL 注入 DWM，并確保 OpenGlass 能正确加载用户配置。
#endif

[Files]
Source: "{#MyAppBuildPath}\OpenGlass.dll"; DestDir: "{app}"; Flags: ignoreversion restartreplace
Source: "{#MyAppBuildPath}\OpenGlassHost.exe"; DestDir: "{app}"; Flags: ignoreversion restartreplace
Source: "{#MyAppBuildPath}\OpenGlassGUI.exe"; DestDir: "{app}"; Flags: ignoreversion restartreplace

[Dirs]
; Writable symbols directory (kept separate from binaries in {app})
Name: "{app}\symbols"

[UninstallDelete]
Type: filesandordirs; Name: "{app}\symbols"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: checkablealone

[Icons]
Name: "{group}\Configure OpenGlass"; Filename: "{app}\OpenGlassGUI.exe"
Name: "{commondesktop}\Configure OpenGlass"; Filename: "{app}\OpenGlassGUI.exe"; Tasks: desktopicon
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"

[Run]
; NOTE: Users can choose an arbitrary install path. If it is a world-writable directory (e.g. C:\OpenGlass),
; we must explicitly harden ACLs so the binary directory is not writable by non-admin users.
;
; SIDs used (language-independent):
;   *S-1-5-18      = LOCAL SYSTEM
;   *S-1-5-32-544  = Administrators
;   *S-1-5-32-545  = Users
;   *S-1-5-90-0    = Window Manager Group (DWM)
;
; Harden {app} (binaries): SYSTEM/Admins = Full, Users = RX, Window Manager = RX
Filename: "{sys}\icacls.exe"; Parameters: """{app}"" /inheritance:r /grant:r *S-1-5-18:(OI)(CI)F *S-1-5-32-544:(OI)(CI)F *S-1-5-32-545:(OI)(CI)RX *S-1-5-90-0:(OI)(CI)RX"; Flags: runhidden waituntilterminated
; Harden OpenGlassHost.exe (service binary): SYSTEM only for execution, but allow Administrators to delete (for uninstallation)
Filename: "{sys}\icacls.exe"; Parameters: """{app}\OpenGlassHost.exe"" /inheritance:r /grant:r *S-1-5-18:F *S-1-5-32-544:D"; Flags: runhidden waituntilterminated
; Harden {app}\symbols (writable cache): SYSTEM/Admins = Full, Users = R, Window Manager = RWD
Filename: "{sys}\icacls.exe"; Parameters: """{app}\symbols"" /inheritance:r /grant:r *S-1-5-18:(OI)(CI)F *S-1-5-32-544:(OI)(CI)F *S-1-5-32-545:(OI)(CI)R *S-1-5-90-0:(OI)(CI)RWD"; Flags: runhidden waituntilterminated

; Create the service
Filename: "{sys}\sc.exe"; Parameters: "delete OpenGlassHost"; Flags: runhidden waituntilterminated
Filename: "{sys}\sc.exe"; Parameters: "create OpenGlassHost binPath= ""{app}\OpenGlassHost.exe"" displayName= ""OpenGlass Host Service"" start= auto type= own"; Flags: runhidden
; Set description
Filename: "{sys}\sc.exe"; Parameters: "description OpenGlassHost ""{cm:ServiceDescription}"""; Flags: runhidden
; Start the service (Automatic start, no checkbox)
Filename: "{sys}\sc.exe"; Parameters: "start OpenGlassHost"; Flags: runhidden nowait
; Launch GUI (Optional post-install)
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchOpenGlassGUI}"; Flags: postinstall nowait runascurrentuser; Check: not WizardSilent

[UninstallRun]
Filename: "{sys}\sc.exe"; Parameters: "stop OpenGlassHost"; Flags: runhidden waituntilterminated; RunOnceId: "StopService"
Filename: "{sys}\taskkill.exe"; Parameters: "/IM ""{#MyAppExeName}"" /F"; Flags: runhidden; RunOnceId: "KillGUI"
Filename: "{sys}\sc.exe"; Parameters: "delete OpenGlassHost"; Flags: runhidden; RunOnceId: "DeleteService"

[Code]

type
  IMMERSIVE_COLOR_PREFERENCE = record
    color1: LongWord;
    color2: LongWord;
  end;

function GetUserColorPreference(var pcpPreference: IMMERSIVE_COLOR_PREFERENCE; fForceReload: Boolean): HRESULT;
external 'GetUserColorPreference@uxtheme.dll stdcall';

procedure DeleteConfig;
var
  OpenGlassKeys: array of string;
  i: Integer;
  pref: IMMERSIVE_COLOR_PREFERENCE;
begin
  OpenGlassKeys := [
    'ColorizationColor', 'ColorizationColorOverride', 'ColorizationColorInactive',
    'ColorizationAfterglow', 'ColorizationColorBalance', 'ColorizationAfterglowBalance', 'ColorizationBlurBalance',
    'ColorizationAfterglowOverride', 'ColorizationColorBalanceOverride', 'ColorizationAfterglowBalanceOverride', 'ColorizationBlurBalanceOverride',
    'GlassOpacity', 'GlassOpacityInactive',
    'ColorizationColorCaption', 'ColorizationColorCaptionInactive', 'ColorizationColorCaptionMaximized', 'ColorizationColorCaptionInactiveMaximized',
    'ColorizationOpaqueBlend',
    'ColorizationBaseTransparent', 'ColorizationBaseMaximized', 'ColorizationBaseOpaque',
    'ColorizationOpaqueBlendPriority',
    'ColorizationOpacity', 'ColorizationOpacityInactive', 'ColorizationOpacityMaximized', 'ColorizationOpacityInactiveMaximized',
    'GlassType', 'GlassOverrideAccent', 'CustomThemeReflection',
    'ColorizationGlassReflectionIntensity',
    'ColorizationGlassReflectionOpacity', 'ColorizationGlassReflectionOpacityInactive', 'ColorizationGlassReflectionOpacityMaximized', 'ColorizationGlassReflectionOpacityInactiveMaximized',
    'ColorizationGlassReflectionParallaxIntensity', 'ColorizationGlassReflectionPolicy',
    'BlurDeviation', 'BlurOptimization', 'RoundRectRadius',
    'CustomThemeMaterial', 'MaterialOpacity', 'UseDirect3DRendering',
    'CaptionButtons', 'CenterCaption', 'TextGlowMode', 'CustomThemeAtlas', 'DisableModernBorders',
    'DisableGlassOnBattery', 'DisabledHooks', 'GlassSafetyZoneMode'
  ];

  for i := 0 to GetArrayLength(OpenGlassKeys) - 1 do
  begin
    RegDeleteValue(HKCU, 'SOFTWARE\Microsoft\Windows\DWM', OpenGlassKeys[i]);
    RegDeleteValue(HKLM, 'SOFTWARE\Microsoft\Windows\DWM', OpenGlassKeys[i]);
  end;

  // Refresh DWM by calling GetUserColorPreference
  GetUserColorPreference(pref, True);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  CmdTail: string;
  NoDeleteConfigFlag: boolean;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    CmdTail := GetCmdTail;
    NoDeleteConfigFlag := Pos('/nodeleteconfig', LowerCase(CmdTail)) > 0;

    // Handle user configuration deletion
    if UninstallSilent then
    begin
      if not NoDeleteConfigFlag then
      begin
        DeleteConfig;
      end;
    end
    else
    begin
      if MsgBox(CustomMessage('DeleteUserConfig'), mbConfirmation, MB_YESNO) = IDYES then
      begin
        DeleteConfig;
      end;
    end;
  end;
end;
