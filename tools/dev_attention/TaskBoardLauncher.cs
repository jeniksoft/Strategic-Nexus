// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Antonin Jenik

using System;
using System.Diagnostics;
using System.IO;
using System.Windows.Forms;

internal static class TaskBoardLauncher
{
    private const string RelativeBoardScript = @"tools\dev_attention\user_task_board.ps1";

    [STAThread]
    private static int Main(string[] args)
    {
        try
        {
            var repoRoot = ResolveRepoRoot(args);
            var boardScript = Path.Combine(repoRoot, RelativeBoardScript);
            if (!File.Exists(boardScript))
            {
                MessageBox.Show(
                    "Task Board script nebyl nalezen:\n" + boardScript,
                    "Strategic Nexus Task Board",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
                return 2;
            }

            var powershellPath = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.Windows),
                @"System32\WindowsPowerShell\v1.0\powershell.exe");

            if (!File.Exists(powershellPath))
            {
                MessageBox.Show(
                    "Windows PowerShell nebyl nalezen:\n" + powershellPath,
                    "Strategic Nexus Task Board",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
                return 3;
            }

            var startInfo = new ProcessStartInfo
            {
                FileName = powershellPath,
                Arguments = "-NoProfile -STA -ExecutionPolicy Bypass -File " + Quote(boardScript),
                WorkingDirectory = repoRoot,
                UseShellExecute = false,
                CreateNoWindow = false,
                WindowStyle = ProcessWindowStyle.Normal,
            };

            Process.Start(startInfo);
            return 0;
        }
        catch (Exception ex)
        {
            MessageBox.Show(
                ex.Message,
                "Strategic Nexus Task Board",
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
            return 1;
        }
    }

    private static string ResolveRepoRoot(string[] args)
    {
        if (args.Length > 0 && !string.IsNullOrWhiteSpace(args[0]))
        {
            return Path.GetFullPath(args[0]);
        }

        var baseDirectory = AppDomain.CurrentDomain.BaseDirectory;
        var fromToolsDirectory = Path.GetFullPath(Path.Combine(baseDirectory, @"..\.."));
        if (File.Exists(Path.Combine(fromToolsDirectory, RelativeBoardScript)))
        {
            return fromToolsDirectory;
        }

        var currentDirectory = Directory.GetCurrentDirectory();
        if (File.Exists(Path.Combine(currentDirectory, RelativeBoardScript)))
        {
            return currentDirectory;
        }

        throw new InvalidOperationException("Repo root nebyl predan a nelze ho odvodit.");
    }

    private static string Quote(string value)
    {
        return "\"" + value.Replace("\"", "\\\"") + "\"";
    }
}
