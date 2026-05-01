; Inno Setup script for Injury VST3 (Windows)
#define MyAppName       "Injury"
#define MyAppVersion    "1.0.0"
#define MyAppPublisher  "Injury Audio"

[Setup]
AppId={{B6A8E7E2-1F3A-4F4D-9C7B-INJURY00001}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={commoncf64}\VST3
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
DisableDirPage=yes
OutputDir=..\dist
OutputBaseFilename=Injury-Windows-Installer
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
UninstallDisplayName={#MyAppName} {#MyAppVersion}

[Files]
; Copy the entire .vst3 bundle directory into the system VST3 folder
Source: "..\stage\Injury.vst3\*"; DestDir: "{commoncf64}\VST3\Injury.vst3"; \
    Flags: recursesubdirs createallsubdirs ignoreversion

[Messages]
WelcomeLabel2=This will install [name/ver] VST3 plugin on your computer.%n%nThe plugin will be available to all DAWs that scan the system VST3 folder.
