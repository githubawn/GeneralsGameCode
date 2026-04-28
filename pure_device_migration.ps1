# Unified Pure Device Migration Script
# Targets: Replace direct D3D8 device access with DX8Wrapper calls

$files = Get-ChildItem -Recurse -Include *.cpp,*.h | Where-Object { 
    $_.FullName -notmatch "dx8wrapper\.(cpp|h)" -and 
    $_.FullName -notmatch "\\dx8\\" 
}

$replacements = @{
    'SetRenderState' = 'Set_DX8_Render_State'
    'SetTextureStageState' = 'Set_DX8_Texture_Stage_State'
    'SetTexture' = 'Set_DX8_Texture'
    'SetVertexShader' = 'Set_Vertex_Shader'
    'SetPixelShader' = 'Set_Pixel_Shader'
    'Clear' = '_Clear'
    'BeginScene' = '_Begin_Scene'
    'EndScene' = '_End_Scene'
    'DrawIndexedPrimitive' = '_Draw_Indexed_Primitive'
    'DrawPrimitive' = '_Draw_Primitive'
    'SetStreamSource' = '_Set_Stream_Source'
    'SetIndices' = '_Set_Indices'
    'SetTransform' = '_Set_DX8_Transform'
    'GetTransform' = '_Get_DX8_Transform'
    'TestCooperativeLevel' = '_Test_Cooperative_Level'
    'CreateVertexShader' = '_Create_Vertex_Shader'
    'CreatePixelShader' = '_Create_Pixel_Shader'
    'DeleteVertexShader' = '_Delete_Vertex_Shader'
    'DeletePixelShader' = '_Delete_Pixel_Shader'
    'ShowCursor' = '_Show_Cursor'
    'SetCursorProperties' = '_Set_Cursor_Properties'
    'SetCursorPosition' = '_Set_Cursor_Position'
    'Present' = '_Present'
    'SetPixelShaderConstant' = '_Set_Pixel_Shader_Constant'
    'SetVertexShaderConstant' = '_Set_Vertex_Shader_Constant'
    'ProcessVertices' = '_ProcessVertices'
    'CreateIndexBuffer' = '_Create_Index_Buffer'
    'CreateVertexBuffer' = '_Create_Vertex_Buffer'
}

$devVars = @('m_pDev', 'pDev', 'd3dDevice', 'pDevice', 'lpD3DDev')

foreach ($file in $files) {
    $content = Get-Content $file.FullName -Raw
    $changed = $false

    # 1. Handle direct DX8Wrapper::_Get_D3D_Device8()-> calls
    foreach ($key in $replacements.Keys) {
        $target = "DX8Wrapper::_Get_D3D_Device8\(\)->$key"
        $replacement = "DX8Wrapper::$($replacements[$key])"
        if ($content -match $target) {
            $content = $content -replace $target, $replacement
            $changed = $true
        }
    }

    # 2. Handle D3DX utility calls that need the raw pointer
    if ($content -match 'D3DXCreateFont\(') {
        $content = $content -replace 'D3DXCreateFont\(\s*(\w+)\s*,', 'D3DXCreateFont(DX8Wrapper::_Get_D3D_Device8(),'
        $changed = $true
    }

    # 3. Handle existence checks and comparisons for common device variable names
    foreach ($var in $devVars) {
        # if (var) or if (!var)
        $content = $content -replace "\bif\s*\(\s*$var\b", "if (DX8Wrapper::_Get_D3D_Device8()"
        $content = $content -replace "\bif\s*\(\s*!\s*$var\b", "if (!DX8Wrapper::_Get_D3D_Device8()"
        
        # var != nullptr or var == nullptr
        $content = $content -replace "\b$var\s*==\s*nullptr\b", "DX8Wrapper::_Get_D3D_Device8() == nullptr"
        $content = $content -replace "\b$var\s*!=\s*nullptr\b", "DX8Wrapper::_Get_D3D_Device8() != nullptr"
        
        # var != NULL or var == NULL
        $content = $content -replace "\b$var\s*==\s*NULL\b", "DX8Wrapper::_Get_D3D_Device8() == NULL"
        $content = $content -replace "\b$var\s*!=\s*NULL\b", "DX8Wrapper::_Get_D3D_Device8() != NULL"

        # Assertions
        $content = $content -replace "DEBUG_ASSERTCRASH\s*\(\s*$var\s*,", "DEBUG_ASSERTCRASH(DX8Wrapper::_Get_D3D_Device8(),"

        # Method calls on the variable
        foreach ($key in $replacements.Keys) {
            $target = [regex]::Escape("$var`->$key")
            $replacement = "DX8Wrapper::$($replacements[$key])"
            if ($content -match $target) {
                $content = $content -replace $target, $replacement
                $changed = $true
            }
        }
        
        # Remove declarations and assignments
        $content = $content -replace "(?:LPDIRECT3DDEVICE8|IDirect3DDevice8\s*\*)\s*$var\s*=\s*DX8Wrapper::_Get_D3D_Device8\(\);", ""
        $content = $content -replace "$var\s*=\s*DX8Wrapper::_Get_D3D_Device8\(\);", ""
        $content = $content -replace "(?:LPDIRECT3DDEVICE8|IDirect3DDevice8\s*\*)\s*$var\s*;", ""
    }

    if ($changed) {
        Set-Content $file.FullName $content -NoNewline
        Write-Host "Unified Migration: Updated $($file.FullName)"
    }
}
