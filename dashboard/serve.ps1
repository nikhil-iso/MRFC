param(
  [int]$Port = 8732
)

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$rootPrefix = [System.IO.Path]::GetFullPath((Join-Path $scriptRoot "."))
$listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, $Port)

$contentTypes = @{
  ".css"  = "text/css; charset=utf-8"
  ".html" = "text/html; charset=utf-8"
  ".ico"  = "image/x-icon"
  ".jpeg" = "image/jpeg"
  ".jpg"  = "image/jpeg"
  ".js"   = "application/javascript; charset=utf-8"
  ".json" = "application/json; charset=utf-8"
  ".png"  = "image/png"
  ".svg"  = "image/svg+xml"
}

function Send-HttpResponse {
  param(
    [Parameter(Mandatory = $true)]
    [System.Net.Sockets.TcpClient]$Client,

    [Parameter(Mandatory = $true)]
    [byte[]]$Body,

    [Parameter(Mandatory = $true)]
    [string]$ContentType,

    [int]$StatusCode = 200,

    [string]$StatusText = "OK"
  )

  $stream = $Client.GetStream()
  $writer = [System.IO.StreamWriter]::new(
    $stream,
    [System.Text.Encoding]::ASCII,
    1024,
    $true
  )

  try {
    $writer.NewLine = "`r`n"
    $writer.WriteLine("HTTP/1.1 $StatusCode $StatusText")
    $writer.WriteLine("Content-Type: $ContentType")
    $writer.WriteLine("Content-Length: $($Body.Length)")
    $writer.WriteLine("Connection: close")
    $writer.WriteLine()
    $writer.Flush()
    $stream.Write($Body, 0, $Body.Length)
    $stream.Flush()
  } finally {
    $writer.Dispose()
    $Client.Close()
  }
}

function Send-TextResponse {
  param(
    [Parameter(Mandatory = $true)]
    [System.Net.Sockets.TcpClient]$Client,

    [Parameter(Mandatory = $true)]
    [string]$Text,

    [int]$StatusCode = 200,

    [string]$StatusText = "OK"
  )

  $bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
  Send-HttpResponse -Client $Client -Body $bytes -ContentType "text/plain; charset=utf-8" -StatusCode $StatusCode -StatusText $StatusText
}

$listener.Start()

Write-Host "Serving dashboard at http://localhost:$Port/"
Write-Host "Open the URL above in Chrome or Edge. Press Ctrl+C to stop."

try {
  while ($true) {
    $client = $listener.AcceptTcpClient()

    try {
      $stream = $client.GetStream()
      $reader = [System.IO.StreamReader]::new(
        $stream,
        [System.Text.Encoding]::ASCII,
        $false,
        1024,
        $true
      )

      $requestLine = $reader.ReadLine()
      if ([string]::IsNullOrWhiteSpace($requestLine)) {
        Send-TextResponse -Client $client -Text "400 Bad Request" -StatusCode 400 -StatusText "Bad Request"
        continue
      }

      while (($headerLine = $reader.ReadLine()) -ne $null) {
        if ($headerLine -eq "") {
          break
        }
      }

      $parts = $requestLine.Split(" ")
      if ($parts.Length -lt 2 -or $parts[0] -ne "GET") {
        Send-TextResponse -Client $client -Text "405 Method Not Allowed" -StatusCode 405 -StatusText "Method Not Allowed"
        continue
      }

      $requestPath = $parts[1].Split("?")[0]
      $relativePath = [System.Uri]::UnescapeDataString($requestPath.TrimStart("/"))
      if ([string]::IsNullOrWhiteSpace($relativePath)) {
        $relativePath = "index.html"
      }

      $candidatePath = Join-Path $scriptRoot $relativePath
      $resolvedPath = [System.IO.Path]::GetFullPath($candidatePath)

      if (
        -not $resolvedPath.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-Path -LiteralPath $resolvedPath -PathType Leaf)
      ) {
        Send-TextResponse -Client $client -Text "404 Not Found" -StatusCode 404 -StatusText "Not Found"
        continue
      }

      $extension = [System.IO.Path]::GetExtension($resolvedPath).ToLowerInvariant()
      $contentType = $contentTypes[$extension]
      if (-not $contentType) {
        $contentType = "application/octet-stream"
      }

      $bytes = [System.IO.File]::ReadAllBytes($resolvedPath)
      Send-HttpResponse -Client $client -Body $bytes -ContentType $contentType
    } catch {
      if ($client.Connected) {
        Send-TextResponse -Client $client -Text "500 Internal Server Error" -StatusCode 500 -StatusText "Internal Server Error"
      }
    } finally {
      if ($client.Connected) {
        $client.Close()
      }
    }
  }
} finally {
  $listener.Stop()
}
