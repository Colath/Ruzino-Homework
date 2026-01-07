# Test CG solver with mass-spring
Write-Host "Testing CG solver with mass-spring implicit integration..." -ForegroundColor Cyan

Set-Location "C:\Users\Pengfei\WorkSpace\Ruzino\Binaries\Debug"

Write-Host "`nRunning Debug build with CG solver..." -ForegroundColor Yellow
.\headless_render.exe --usd="..\..\Assets\soft_body.usdc" --json="..\..\Assets\render_nodes_save.json" --output="test_cg.png" --width=800 --height=600 --spp=1 --frames=1 --no-save --verbose

Write-Host "`n========================================" -ForegroundColor Green
Write-Host "Test completed!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
