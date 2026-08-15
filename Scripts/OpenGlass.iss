; Keep the product identity in one place. The outer doubled braces preserve
; the literal GUID braces after the preprocessor expands MyAppId.
#define MyAppId "{D3D1BC7D-5E24-4B33-9383-7934271A3B05}"
#define MyAppName "OpenGlass"
#ifndef MyAppVersion
#error MyAppVersion must be supplied by the packaging target
#endif
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
#ifndef OutputPath
#define OutputPath MyAppBuildPath
#endif
#define MyAppOutputPath OutputPath

[Setup]
AppId={{#MyAppId}}
AppName={#MyAppName}
VersionInfoVersion={#MyAppVersion}
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
OutputDir={#MyAppOutputPath}
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
UninstallOptionsTitle=OpenGlass uninstall options
UninstallOptionsDescription=Choose the OpenGlass data to remove.
UninstallDeleteCurrentAndMachineConfig=Delete current-user and machine configuration
OtherUserConfigNotice=Settings in other Windows user profiles are preserved and must be removed manually while signed in as that user.
UninstallDeletePresetPackages=Delete installed OpenGlass preset packages
ContinueUninstall=Uninstall
ServiceDescription=This service injects DLL into DWM for you and also maintains that user settings are correctly loaded.

#if FileExists(CompilerPath + "Languages\\ChineseSimplified.isl")
chinesesimplified.LaunchOpenGlassGUI=启动 OpenGlass GUI
chinesesimplified.UninstallOptionsTitle=OpenGlass 卸载选项
chinesesimplified.UninstallOptionsDescription=选择要删除的 OpenGlass 数据。
chinesesimplified.UninstallDeleteCurrentAndMachineConfig=删除当前用户和本机的配置
chinesesimplified.OtherUserConfigNotice=其他 Windows 用户配置文件中的设置将被保留，需登录相应用户后手动删除。
chinesesimplified.UninstallDeletePresetPackages=删除已安装的 OpenGlass 预设包
chinesesimplified.ContinueUninstall=卸载
chinesesimplified.ServiceDescription=该服务会为您将 DLL 注入 DWM，并确保 OpenGlass 能正确加载用户配置。
#endif

#if FileExists(CompilerPath + "Languages\\ChineseTraditional.isl")
chinesestraditional.LaunchOpenGlassGUI=啟動 OpenGlass GUI
chinesestraditional.UninstallOptionsTitle=OpenGlass 卸載選項
chinesestraditional.UninstallOptionsDescription=選擇要刪除的 OpenGlass 資料。
chinesestraditional.UninstallDeleteCurrentAndMachineConfig=刪除目前使用者與本機的設定
chinesestraditional.OtherUserConfigNotice=其他 Windows 使用者設定檔中的設定會予以保留；需登入該使用者後手動刪除。
chinesestraditional.UninstallDeletePresetPackages=刪除已安裝的 OpenGlass 預設套件
chinesestraditional.ContinueUninstall=卸載
chinesestraditional.ServiceDescription=該服務負責為您將 DLL 注入 DWM，並確保 OpenGlass 能正確載入使用者設定。
#endif

[Files]
Source: "{#MyAppBuildPath}\legacy\OpenGlass.dll"; DestDir: "{app}"; DestName: "OpenGlass.dll"; Flags: ignoreversion restartreplace; Check: ShouldInstallLegacyDll
Source: "{#MyAppBuildPath}\milcomp\OpenGlass.dll"; DestDir: "{app}"; DestName: "OpenGlass.dll"; Flags: ignoreversion restartreplace; Check: ShouldInstallMILCompDll
Source: "{#MyAppBuildPath}\OpenGlassHost.exe"; DestDir: "{app}"; Flags: ignoreversion restartreplace
Source: "{#MyAppBuildPath}\OpenGlassGUI.exe"; DestDir: "{app}"; Flags: ignoreversion restartreplace

[Dirs]
; Mutable application data and immutable preset deployments live outside the installation directory.
Name: "{commonappdata}\OpenGlass"
Name: "{commonappdata}\OpenGlass\symbols"
; Full DWM dumps can contain sensitive process memory; do not grant ordinary users access.
Name: "{commonappdata}\OpenGlass\dumps"
Name: "{commonappdata}\OpenGlass\Presets"

[UninstallDelete]
Type: filesandordirs; Name: "{commonappdata}\OpenGlass\symbols"
Type: filesandordirs; Name: "{commonappdata}\OpenGlass\dumps"
; Clean the defaults used by older installers without touching any user-selected custom path.
Type: filesandordirs; Name: "{app}\symbols"
Type: filesandordirs; Name: "{app}\dumps"

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
; Harden the shared data root before applying narrower permissions to each child.
Filename: "{sys}\icacls.exe"; Parameters: """{commonappdata}\OpenGlass"" /inheritance:r /grant:r *S-1-5-18:(OI)(CI)F *S-1-5-32-544:(OI)(CI)F *S-1-5-32-545:(OI)(CI)RX *S-1-5-90-0:(OI)(CI)RX"; Flags: runhidden waituntilterminated
; Symbols: SYSTEM/Admins = Full, Users = Read, Window Manager = Read/Write/Delete.
Filename: "{sys}\icacls.exe"; Parameters: """{commonappdata}\OpenGlass\symbols"" /inheritance:r /grant:r *S-1-5-18:(OI)(CI)F *S-1-5-32-544:(OI)(CI)F *S-1-5-32-545:(OI)(CI)R *S-1-5-90-0:(OI)(CI)RWD"; Flags: runhidden waituntilterminated
; Dumps: SYSTEM/Admins = Full, Window Manager = Modify; no ordinary-user access to dump contents.
Filename: "{sys}\icacls.exe"; Parameters: """{commonappdata}\OpenGlass\dumps"" /inheritance:r /grant:r *S-1-5-18:(OI)(CI)F *S-1-5-32-544:(OI)(CI)F *S-1-5-90-0:(OI)(CI)M"; Flags: runhidden waituntilterminated
; Presets: SYSTEM/Admins = Full, Users/Window Manager = Read/Execute. Package directories retain the same protected ACL.
Filename: "{sys}\icacls.exe"; Parameters: """{commonappdata}\OpenGlass\Presets"" /inheritance:r /grant:r *S-1-5-18:(OI)(CI)F *S-1-5-32-544:(OI)(CI)F *S-1-5-32-545:(OI)(CI)RX *S-1-5-90-0:(OI)(CI)RX"; Flags: runhidden waituntilterminated

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

var
  InstallMILCompDll: Boolean;

function InitializeSetup(): Boolean;
var
  Version: TWindowsVersion;
begin
  GetWindowsVersionEx(Version);
  InstallMILCompDll := Version.Build >= 28000;
  if InstallMILCompDll then
    Log(Format('Detected Windows build %d; selected the MILComp DWM architecture.', [Version.Build]))
  else
    Log(Format('Detected Windows build %d; selected the Legacy DWM architecture.', [Version.Build]));
  Result := True;
end;

function ShouldInstallLegacyDll(): Boolean;
begin
  Result := not InstallMILCompDll;
end;

function ShouldInstallMILCompDll(): Boolean;
begin
  Result := InstallMILCompDll;
end;

type
  IMMERSIVE_COLOR_PREFERENCE = record
    color1: LongWord;
    color2: LongWord;
  end;

function GetUserColorPreference(var pcpPreference: IMMERSIVE_COLOR_PREFERENCE; fForceReload: Boolean): HRESULT;
external 'GetUserColorPreference@uxtheme.dll stdcall';

var
  DeleteCurrentAndMachineConfigOnUninstall: Boolean;
  DeletePresetsOnUninstall: Boolean;

procedure DeleteConfigValues(const RootKey: Integer; const SubKey: String; const OpenGlassKeys: TArrayOfString);
var
  i: Integer;
begin
  for i := 0 to GetArrayLength(OpenGlassKeys) - 1 do
  begin
    if RegValueExists(RootKey, SubKey, OpenGlassKeys[i]) and
       (not RegDeleteValue(RootKey, SubKey, OpenGlassKeys[i])) then
      Log(Format('Unable to delete OpenGlass registry value %s\%s', [SubKey, OpenGlassKeys[i]]));
  end;
end;

// "Current user" here means the account the elevated uninstaller runs as.
// When uninstall is started from a non-admin account (UAC prompt / run-as),
// HKEY_CURRENT_USER is that admin account, not the launching user; that
// user's settings then remain and are only reachable by logging in as them.
procedure DeleteConfig;
var
  OpenGlassKeys: array of string;
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
    'DisableGlassOnBattery', 'DisabledHooks', 'GlassSafetyZoneMode',
    'MINMAXBUTTONGLOWid', 'CLOSEBUTTONGLOWid', 'TOOLCLOSEBUTTONGLOWid'
  ];

  DeleteConfigValues(HKEY_CURRENT_USER, 'SOFTWARE\Microsoft\Windows\DWM', OpenGlassKeys);
  DeleteConfigValues(HKEY_LOCAL_MACHINE, 'SOFTWARE\Microsoft\Windows\DWM', OpenGlassKeys);

  // Refresh DWM by calling GetUserColorPreference
  GetUserColorPreference(pref, True);
end;

procedure InitializeUninstallProgressForm;
var
  OptionsPage: TNewNotebookPage;
  DeleteCurrentAndMachineConfigCheck: TNewCheckBox;
  OtherUserConfigNotice: TNewStaticText;
  DeletePresetsCheck: TNewCheckBox;
  ContinueButton: TNewButton;
  OriginalPageName: String;
  OriginalPageDescription: String;
  OriginalCancelEnabled: Boolean;
  OriginalCancelModalResult: Integer;
  DialogResult: Integer;
begin
  // Interactive uninstalls present the options page inside the progress
  // window; silent (/SILENT or /VERYSILENT) uninstalls stay fully silent.
  if UninstallSilent then
    Exit;

  OptionsPage := TNewNotebookPage.Create(UninstallProgressForm);
  OptionsPage.Notebook := UninstallProgressForm.InnerNotebook;
  OptionsPage.Align := alClient;

  DeleteCurrentAndMachineConfigCheck := TNewCheckBox.Create(UninstallProgressForm);
  DeleteCurrentAndMachineConfigCheck.Parent := OptionsPage;
  DeleteCurrentAndMachineConfigCheck.Left := UninstallProgressForm.StatusLabel.Left;
  DeleteCurrentAndMachineConfigCheck.Top := UninstallProgressForm.StatusLabel.Top;
  DeleteCurrentAndMachineConfigCheck.Width := UninstallProgressForm.StatusLabel.Width;
  DeleteCurrentAndMachineConfigCheck.Height := ScaleY(20);
  DeleteCurrentAndMachineConfigCheck.Caption :=
    CustomMessage('UninstallDeleteCurrentAndMachineConfig');
  DeleteCurrentAndMachineConfigCheck.Checked :=
    DeleteCurrentAndMachineConfigOnUninstall;

  OtherUserConfigNotice := TNewStaticText.Create(UninstallProgressForm);
  OtherUserConfigNotice.Parent := OptionsPage;
  OtherUserConfigNotice.Left :=
    DeleteCurrentAndMachineConfigCheck.Left + ScaleX(22);
  OtherUserConfigNotice.Top :=
    DeleteCurrentAndMachineConfigCheck.Top + ScaleY(24);
  OtherUserConfigNotice.Width :=
    DeleteCurrentAndMachineConfigCheck.Width - ScaleX(22);
  OtherUserConfigNotice.Height := ScaleY(34);
  OtherUserConfigNotice.AutoSize := False;
  OtherUserConfigNotice.WordWrap := True;
  OtherUserConfigNotice.Caption := CustomMessage('OtherUserConfigNotice');

  DeletePresetsCheck := TNewCheckBox.Create(UninstallProgressForm);
  DeletePresetsCheck.Parent := OptionsPage;
  DeletePresetsCheck.Left := DeleteCurrentAndMachineConfigCheck.Left;
  DeletePresetsCheck.Top :=
    DeleteCurrentAndMachineConfigCheck.Top + ScaleY(62);
  DeletePresetsCheck.Width := DeleteCurrentAndMachineConfigCheck.Width;
  DeletePresetsCheck.Height := ScaleY(20);
  DeletePresetsCheck.Caption :=
    CustomMessage('UninstallDeletePresetPackages');
  DeletePresetsCheck.Checked := DeletePresetsOnUninstall;

  ContinueButton := TNewButton.Create(UninstallProgressForm);
  ContinueButton.Parent := UninstallProgressForm;
  ContinueButton.Caption := CustomMessage('ContinueUninstall');
  ContinueButton.Left :=
    UninstallProgressForm.CancelButton.Left -
    ScaleX(8) -
    UninstallProgressForm.CancelButton.Width;
  ContinueButton.Top := UninstallProgressForm.CancelButton.Top;
  ContinueButton.Width := UninstallProgressForm.CancelButton.Width;
  ContinueButton.Height := UninstallProgressForm.CancelButton.Height;
  ContinueButton.Anchors := [akRight, akBottom];
  ContinueButton.ModalResult := mrOk;
  ContinueButton.Default := True;

  OriginalPageName := UninstallProgressForm.PageNameLabel.Caption;
  OriginalPageDescription :=
    UninstallProgressForm.PageDescriptionLabel.Caption;
  OriginalCancelEnabled := UninstallProgressForm.CancelButton.Enabled;
  OriginalCancelModalResult :=
    UninstallProgressForm.CancelButton.ModalResult;

  UninstallProgressForm.PageNameLabel.Caption :=
    CustomMessage('UninstallOptionsTitle');
  UninstallProgressForm.PageDescriptionLabel.Caption :=
    CustomMessage('UninstallOptionsDescription');
  UninstallProgressForm.InnerNotebook.ActivePage := OptionsPage;
  UninstallProgressForm.CancelButton.Enabled := True;
  UninstallProgressForm.CancelButton.ModalResult := mrCancel;
  UninstallProgressForm.ActiveControl := ContinueButton;

  DialogResult := UninstallProgressForm.ShowModal;
  if DialogResult = mrOk then
  begin
    DeleteCurrentAndMachineConfigOnUninstall :=
      DeleteCurrentAndMachineConfigCheck.Checked;
    DeletePresetsOnUninstall := DeletePresetsCheck.Checked;
  end;

  ContinueButton.Visible := False;
  UninstallProgressForm.PageNameLabel.Caption := OriginalPageName;
  UninstallProgressForm.PageDescriptionLabel.Caption :=
    OriginalPageDescription;
  UninstallProgressForm.CancelButton.Enabled := OriginalCancelEnabled;
  UninstallProgressForm.CancelButton.ModalResult :=
    OriginalCancelModalResult;
  UninstallProgressForm.InnerNotebook.ActivePage :=
    UninstallProgressForm.InstallingPage;

  if DialogResult = mrCancel then
    Abort;
end;

function InitializeUninstall(): Boolean;
var
  CmdTail: String;
begin
  CmdTail := LowerCase(GetCmdTail);
  DeleteCurrentAndMachineConfigOnUninstall :=
    Pos('/nodeleteconfig', CmdTail) = 0;
  DeletePresetsOnUninstall := Pos('/deletepresets', CmdTail) > 0;
  Result := True;
end;

procedure CurUninstallStepChanged(
  CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
  begin
    if DeleteCurrentAndMachineConfigOnUninstall then
      DeleteConfig;

    // Preset packages are user-created/downloaded content and are preserved by default.
    if DeletePresetsOnUninstall then
      DelTree(
        ExpandConstant('{commonappdata}\OpenGlass\Presets'),
        True,
        True,
        True);

    // Remove the shared root only when no preserved package or undeleted runtime data remains.
    RemoveDir(ExpandConstant('{commonappdata}\OpenGlass'));
  end;
end;
