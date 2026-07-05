# scripts/make_iso.ps1
# Creates a standard ISO-9660 image of the Switch package using Windows IMAPI2 API.

param(
    [string]$SourceFolder,
    [string]$OutputFile,
    [string]$VolumeLabel = "GENERALSZH"
)

$ErrorActionPreference = "Stop"

# C# helper class to copy COM IStream to a .NET FileStream
$csharpSource = @"
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;

public class ComStreamCopier {
    public static void CopyToFile(object comStream, string filePath) {
        IStream source = (IStream)comStream;
        using (FileStream dest = File.Create(filePath)) {
            byte[] buffer = new byte[1024 * 1024]; // 1MB buffer
            IntPtr bytesReadPtr = Marshal.AllocHGlobal(sizeof(int));
            try {
                while (true) {
                    source.Read(buffer, buffer.Length, bytesReadPtr);
                    int bytesRead = Marshal.ReadInt32(bytesReadPtr);
                    if (bytesRead == 0) break;
                    dest.Write(buffer, 0, bytesRead);
                }
            } finally {
                Marshal.FreeHGlobal(bytesReadPtr);
            }
        }
    }
}
"@

# Add the C# helper type to the session
try {
    Add-Type -TypeDefinition $csharpSource -ErrorAction SilentlyContinue
} catch {}

Write-Host "Creating ISO image from: $SourceFolder"
Write-Host "Output ISO path: $OutputFile"

# Initialize IMAPI2 FileSystemImage
$image = New-Object -ComObject IMapi2FS.MsftFileSystemImage
$image.VolumeName = $VolumeLabel
$image.FileSystemsToCreate = 1 + 2 # ISO9660 + Joliet
$image.FreeMediaBlocks = 10000000 # Increase capacity to approx 20GB for large assets

$root = $image.Root

# Recursively add the entire folder tree in one call
Write-Host "Adding files and directories to the image..."
$root.AddTree($SourceFolder, $false)

# Create the result image stream
Write-Host "Generating filesystem stream..."
$result = $image.CreateResultImage()
$stream = $result.ImageStream

# Write stream to the output ISO file
Write-Host "Writing ISO to disk (this can take a few moments)..."
[ComStreamCopier]::CopyToFile($stream, $OutputFile)

Write-Host "ISO created successfully: $OutputFile"
