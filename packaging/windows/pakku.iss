; Windows installer — Inno Setup 6.
;
;   iscc packaging\windows\pakku.iss
;   iscc /DAppVersion=1.0.0 /DBuildDir=..\..\build packaging\windows\pakku.iss
;
; VST3 only: Audio Unit is a macOS format. JUCE builds VST3 as a bundle
; folder, which is why the copy below is recursive.

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\..\build"
#endif

#define AppName     "Pakku"
#define AppPublisher "Kyantech Labs"
#define AppURL      "https://github.com/danielalves96/pakku-vst"
#define Vst3Source  BuildDir + "\src\Pakku_artefacts\Release\VST3\Pakku.vst3"

[Setup]
AppId={{7C4E2E4A-9E1B-4F27-8E2B-2C7A5D1F9A31}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}/issues
AppUpdatesURL={#AppURL}/releases
VersionInfoVersion={#AppVersion}

; VST3 lives in a shared system folder, so this needs elevation
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
CreateAppDir=no
Uninstallable=yes
UninstallFilesDir={commonappdata}\{#AppPublisher}\{#AppName}

LicenseFile=..\..\LICENSE
OutputDir=..\..\dist
OutputBaseFilename={#AppName}-{#AppVersion}-Setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
SetupIconFile=
DisableWelcomePage=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Messages]
WelcomeLabel2=This will install [name/ver] on your computer.%n%nPakku is a multiband transient shaper. The VST3 plug-in goes to the shared VST3 folder, where every host looks for it.%n%nRescan your plug-ins afterwards — most hosts only look on startup.

[Files]
Source: "{#Vst3Source}\*"; DestDir: "{commoncf64}\VST3\Pakku.vst3"; \
    Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Pakku.vst3"

[Code]
// User presets live in %APPDATA%\Kyantech Labs\Pakku and are left alone:
// they belong to the user, not to the installer.
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
    MsgBox('Pakku was removed.'#13#10#13#10 +
           'Your presets were kept in:'#13#10 +
           ExpandConstant('{userappdata}') + '\Kyantech Labs\Pakku',
           mbInformation, MB_OK);
end;
