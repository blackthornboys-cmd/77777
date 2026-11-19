[Setup]
AppName=FrozenTune v4.0 (frozenmyheart)
AppVersion=3.0.0
AppPublisher=frozenmyheart
AppPublisherURL=https://t.me/heartmyfrozen
AppSupportURL=https://t.me/heartmyfrozen
DefaultDirName=C:\Program Files\Common Files\VST3
DisableDirPage=no
DisableProgramGroupPage=yes
PrivilegesRequired=admin
OutputDir=..\dist
OutputBaseFilename=FrozenTune_Setup_frozenmyheart
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "..\build\FrozenTune_artefacts\Release\VST3\FrozenTune.vst3\*"; DestDir: "{app}\FrozenTune.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[Run]
Filename: "https://t.me/heartmyfrozen"; Description: "Открыть Telegram автора — t.me/heartmyfrozen (frozenmyheart)"; Flags: shellexec runasoriginaluser postinstall unchecked

[Messages]
WelcomeLabel1=Установка FrozenTune v4.0
WelcomeLabel2=Студийный вокальный тюнер от frozenmyheart (t.me/heartmyfrozen): Pitch Reference (415–465 Hz), Speed до -5.0, XY Doubler, Note Stabilizer и Low Latency 1.5 ms.
