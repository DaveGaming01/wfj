@echo off
rem Installs the dcsswiftbus export block into your DCS Saved Games profile(s).
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install-export-lua.ps1"
pause
