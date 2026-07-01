!include "MUI2.nsh"

Name "Chills Music Visualizer"
OutFile "Chills_Visualizer_Setup.exe"
Unicode True
RequestExecutionLevel admin
InstallDir "$PROGRAMFILES64\Chills Visualizer"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section "Install"
  SetOutPath "$INSTDIR"

  File "visualizer.exe"
  File "vertex_shader.glsl"
  File "fragment_shader.glsl"

  CreateShortCut "$DESKTOP\Chills Visualizer.lnk" "$INSTDIR\visualizer.exe"
  CreateDirectory "$SMPROGRAMS\Chills Visualizer"
  CreateShortCut "$SMPROGRAMS\Chills Visualizer\Chills Visualizer.lnk" "$INSTDIR\visualizer.exe"
  CreateShortCut "$SMPROGRAMS\Chills Visualizer\Uninstall.lnk" "$INSTDIR\uninstall.exe"

  WriteUninstaller "$INSTDIR\uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ChillsVisualizer" \
                   "DisplayName" "Chills Music Visualizer"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ChillsVisualizer" \
                   "UninstallString" "$\"$INSTDIR\uninstall.exe$\""
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\visualizer.exe"
  Delete "$INSTDIR\vertex_shader.glsl"
  Delete "$INSTDIR\fragment_shader.glsl"
  Delete "$INSTDIR\uninstall.exe"
  RMDir "$INSTDIR"

  Delete "$DESKTOP\Chills Visualizer.lnk"
  Delete "$SMPROGRAMS\Chills Visualizer\Chills Visualizer.lnk"
  Delete "$SMPROGRAMS\Chills Visualizer\Uninstall.lnk"
  RMDir "$SMPROGRAMS\Chills Visualizer"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ChillsVisualizer"
SectionEnd
