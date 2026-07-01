!include "MUI2.nsh"

Name "vizl"
OutFile "vizl_Setup.exe"
Unicode True
RequestExecutionLevel admin
InstallDir "$PROGRAMFILES64\vizl"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section "Install"
  SetOutPath "$INSTDIR"

  File "vizl.exe"
  File "vertex_shader.glsl"
  File "fragment_shader.glsl"
  File "image.vert"
  File "image.frag"

  CreateShortCut "$DESKTOP\vizl.lnk" "$INSTDIR\vizl.exe"
  CreateDirectory "$SMPROGRAMS\vizl"
  CreateShortCut "$SMPROGRAMS\vizl\vizl.lnk" "$INSTDIR\vizl.exe"
  CreateShortCut "$SMPROGRAMS\vizl\Uninstall.lnk" "$INSTDIR\uninstall.exe"

  WriteUninstaller "$INSTDIR\uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vizl" \
                   "DisplayName" "vizl"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vizl" \
                   "UninstallString" "$\"$INSTDIR\uninstall.exe$\""
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\vizl.exe"
  Delete "$INSTDIR\vertex_shader.glsl"
  Delete "$INSTDIR\fragment_shader.glsl"
  Delete "$INSTDIR\image.vert"
  Delete "$INSTDIR\image.frag"
  Delete "$INSTDIR\uninstall.exe"
  RMDir "$INSTDIR"

  Delete "$DESKTOP\vizl.lnk"
  Delete "$SMPROGRAMS\vizl\vizl.lnk"
  Delete "$SMPROGRAMS\vizl\Uninstall.lnk"
  RMDir "$SMPROGRAMS\vizl"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vizl"
SectionEnd
