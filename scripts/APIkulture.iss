; Per-user installer: no UAC / Administrator prompt.
; Requires Inno Setup 6 (https://jrsoftware.org/isinfo.php).
; Build from this directory (where apikulture.exe and slint_cpp.dll live):
;   ISCC APIkulture.iss
; or run build-installer.cmd

#define MyAppName "APIkulture"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Sheldonari"
#define MyAppExeName "apikulture.exe"

[Setup]
AppId={{8F3E1C2A-9B7D-4E61-A5C4-1D0E7F6A2B98}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL=https://codeberg.org/Sheldonari/APIKulture
DefaultDirName={localappdata}\Programs\{#MyAppName}
DefaultGroupName={#MyAppName}
; Install only under the current user profile (typically ...\AppData\Local\Programs\...).
; Does not write to Program Files or HKLM.
PrivilegesRequired=lowest
OutputDir=dist
OutputBaseFilename=APIkulture-{#MyAppVersion}-Windows-userSetup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#MyAppExeName}
DisableWelcomePage=no
DisableDirPage=no
DisableProgramGroupPage=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "slint_cpp.dll"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{userdesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
