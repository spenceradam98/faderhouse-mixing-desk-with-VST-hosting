; Windows installer for Faderhouse and Faderhouse Strip (Inno Setup 6).
; Build: iscc /DAppVersion=0.2.0 /DBuild=<path to build dir> /DOutDir=<dir> Faderhouse.iss

#ifndef AppVersion
  #define AppVersion "0.2.0"
#endif
#ifndef Build
  #define Build "..\..\build"
#endif
#ifndef OutDir
  #define OutDir "..\..\dist"
#endif
#define Art Build + "\Faderhouse_artefacts\Release"
#define StripArt Build + "\FaderhouseStrip_artefacts\Release"

[Setup]
AppId={{A2E94D17-6C3B-4F0A-8E25-1D7B9C4F6A38}
AppName=Faderhouse
AppVersion={#AppVersion}
AppPublisher=YourBrand
DefaultDirName={autopf}\YourBrand\Faderhouse
DefaultGroupName=YourBrand
DisableProgramGroupPage=yes
OutputDir={#OutDir}
OutputBaseFilename=Faderhouse-{#AppVersion}-windows-setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName=Faderhouse {#AppVersion}
InfoBeforeFile=readme-before.txt

[Types]
Name: "full"; Description: "Full install"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "Faderhouse VST3 (the 16-channel desk)"; Types: full custom
Name: "strip"; Description: "Faderhouse Strip VST3 (one channel, per-track insert)"; Types: full custom
Name: "app"; Description: "Faderhouse standalone app (hosts VST3 instruments and effects)"; Types: full

[Files]
Source: "{#Art}\VST3\Faderhouse.vst3\*"; DestDir: "{commoncf64}\VST3\Faderhouse.vst3"; Components: vst3; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#StripArt}\VST3\Faderhouse Strip.vst3\*"; DestDir: "{commoncf64}\VST3\Faderhouse Strip.vst3"; Components: strip; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#Art}\Standalone\Faderhouse.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Faderhouse"; Filename: "{app}\Faderhouse.exe"; Components: app
Name: "{autodesktop}\Faderhouse"; Filename: "{app}\Faderhouse.exe"; Components: app; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut for the standalone app"; Components: app; Flags: unchecked

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Faderhouse.vst3"
Type: filesandordirs; Name: "{commoncf64}\VST3\Faderhouse Strip.vst3"
