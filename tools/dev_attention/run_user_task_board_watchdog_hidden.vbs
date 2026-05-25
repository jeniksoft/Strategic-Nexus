Option Explicit

Dim fso
Dim shell
Dim scriptDir
Dim repoRoot
Dim watchdogScript
Dim powershellPath
Dim command
Dim existingTerminalIds

Set fso = CreateObject("Scripting.FileSystemObject")
Set shell = CreateObject("WScript.Shell")
Set existingTerminalIds = CreateObject("Scripting.Dictionary")

Sub RememberTerminalProcesses()
    Dim wmi
    Dim processes
    Dim process

    Set wmi = GetObject("winmgmts:\\.\root\cimv2")
    Set processes = wmi.ExecQuery("SELECT ProcessId FROM Win32_Process WHERE Name='WindowsTerminal.exe' OR Name='OpenConsole.exe'")
    For Each process In processes
        existingTerminalIds(CStr(process.ProcessId)) = True
    Next
End Sub

Sub CloseNewTerminalProcesses()
    Dim wmi
    Dim processes
    Dim process

    Set wmi = GetObject("winmgmts:\\.\root\cimv2")
    Set processes = wmi.ExecQuery("SELECT ProcessId FROM Win32_Process WHERE Name='WindowsTerminal.exe' OR Name='OpenConsole.exe'")
    For Each process In processes
        If Not existingTerminalIds.Exists(CStr(process.ProcessId)) Then
            process.Terminate()
        End If
    Next
End Sub

scriptDir = fso.GetParentFolderName(WScript.ScriptFullName)
repoRoot = fso.GetParentFolderName(fso.GetParentFolderName(scriptDir))
watchdogScript = fso.BuildPath(scriptDir, "run_user_task_board_watchdog.ps1")
powershellPath = shell.ExpandEnvironmentStrings("%SystemRoot%") & "\System32\WindowsPowerShell\v1.0\powershell.exe"

command = """" & powershellPath & """ -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File """ & watchdogScript & """"
shell.CurrentDirectory = repoRoot
RememberTerminalProcesses
shell.Run command, 0, False
WScript.Sleep 1000
CloseNewTerminalProcesses
WScript.Sleep 1500
CloseNewTerminalProcesses
WScript.Sleep 2500
CloseNewTerminalProcesses
